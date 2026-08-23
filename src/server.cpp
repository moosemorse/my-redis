#include "buffer.hpp"
#include "hashtable.hpp"
#include "shared.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#define container_of(ptr, T, member) ((T *)((char *)ptr - offsetof(T, member)))

struct Conn {
  int fd{-1};
  bool want_read{false};
  bool want_write{false};
  bool want_close{false};
  Buffer incoming;
  Buffer outgoing;
};

static struct {
  HMap db;
} g_data;

struct Entry {
  struct HNode node;
  std::string key;
  std::string val;
};

// error code for TAG_ERR
enum {
    ERR_UNKNOWN = 1,    // unknown command
    ERR_TOO_BIG = 2,    // response too big
};

// data types of serialized data
enum {
    TAG_NIL = 0,    // nil
    TAG_ERR = 1,    // error code + msg
    TAG_STR = 2,    // string
    TAG_INT = 3,    // int64
    TAG_DBL = 4,    // double
    TAG_ARR = 5,    // array
};

// help functions for the serialization
static void buf_append_u8(Buffer &buf, uint8_t data) {
    buf.append(&data, 1);
}
static void buf_append_u32(Buffer &buf, uint32_t data) {
    buf.append((const uint8_t *)&data, 4);
}
static void buf_append_i64(Buffer &buf, int64_t data) {
    buf.append((const uint8_t *)&data, 8);
}
static void buf_append_dbl(Buffer &buf, double data) {
    buf.append((const uint8_t *)&data, 8);
}

// append serialized data types to the back
static void out_nil(Buffer &out) {
    buf_append_u8(out, TAG_NIL);
}
static void out_str(Buffer &out, const char *s, size_t size) {
    buf_append_u8(out, TAG_STR);
    buf_append_u32(out, (uint32_t)size);
    out.append((const uint8_t *)s, size);
}
static void out_int(Buffer &out, int64_t val) {
    buf_append_u8(out, TAG_INT);
    buf_append_i64(out, val);
}
static void out_dbl(Buffer &out, double val) {
    buf_append_u8(out, TAG_DBL);
    buf_append_dbl(out, val);
}
static void out_err(Buffer &out, uint32_t code, const std::string &msg) {
    buf_append_u8(out, TAG_ERR);
    buf_append_u32(out, code);
    buf_append_u32(out, (uint32_t)msg.size());
    out.append((const uint8_t *)msg.data(), msg.size());
}
static void out_arr(Buffer &out, uint32_t n) {
    buf_append_u8(out, TAG_ARR);
    buf_append_u32(out, n);
}


static bool cb_keys(HNode *node, void *arg) {
    Buffer &out = *(Buffer *)arg;
    const std::string &key = container_of(node, Entry, node)->key;
    out_str(out, key.data(), key.size());
    return true;
}

static void do_keys(std::vector<std::string> &, Buffer &out) {
    out_arr(out, (uint32_t)hm_size(&g_data.db));
    hm_foreach(&g_data.db, &cb_keys, (void *)&out);
}

static void response_begin(Buffer &out, size_t *header) {
    *header = out.size();       // messege header position
    buf_append_u32(out, 0);     // reserve space
}
static size_t response_size(Buffer &out, size_t header) {
    return out.size() - header - 4;
}
static void response_end(Buffer &out, size_t header) {
    size_t msg_size = response_size(out, header);
    if (msg_size > k_max_msg) {
        out.truncate(header + 4);
        out_err(out, ERR_TOO_BIG, "response is too big.");
        msg_size = response_size(out, header);
    }
    // message header
    uint32_t len = (uint32_t)msg_size;
    memcpy(out.mutable_data() + header, &len, 4);
}

static void fd_set_nb(int fd);
static Conn *handle_accept(int fd);
static bool try_one_request(Conn *conn);
static void handle_write(Conn *conn);
static void handle_read(Conn *conn);
static int32_t parse_req(const uint8_t *data, size_t size,
                         std::vector<std::string> &out);
static bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out);
static bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n,
                     std::string &out);
static void do_request_with_hm(std::vector<std::string> &cmd, Buffer &out);
static uint64_t str_hash(const uint8_t *data, size_t len);
static bool entry_eq(HNode *lhs, HNode *rhs);
static void do_get(std::vector<std::string> &cmd, Buffer &out);
static void do_set(std::vector<std::string> &cmd, Buffer &out);
static void do_del(std::vector<std::string> &cmd, Buffer &out);

const size_t k_max_args = 1024; // max number of arguments in a request

// TODO: note for later to replace poll() with epoll()

int main() {
  /// (1) creating and configure socket

  int fd = socket(AF_INET, SOCK_STREAM, 0);

  int val{1};
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  // (2) bind socket to address

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(k_port);
  addr.sin_addr.s_addr = htonl(0);
  if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr))) {
    throw std::runtime_error("bind() error");
  }

  // (3) listen for incoming connections

  if (listen(fd, SOMAXCONN)) {
    throw std::runtime_error("listen() error");
  }

  // (4) accept connections and process

  std::vector<Conn *> fd2conn;
  std::vector<struct pollfd> poll_args;

  while (true) {

    // prepare the arguments for poll() -- mapping fd to Conn*
    poll_args.clear();
    struct pollfd pfd = {fd, POLLIN, 0};
    poll_args.push_back(pfd);
    for (Conn *conn : fd2conn) {
      if (!conn) {
        continue;
      }
      struct pollfd pfd = {conn->fd, POLLERR, 0};
      if (conn->want_read) {
        pfd.events |= POLLIN;
      }
      if (conn->want_write) {
        pfd.events |= POLLOUT;
      }
      poll_args.push_back(pfd);
    }

    int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
    if (rv < 0 && errno == EINTR) {
      errno = 0; // not an error
      continue;
    }
    if (rv < 0)
      throw std::runtime_error("poll() error");

    // 0 is listening socket, 1..N are client connections
    if (poll_args[0].revents) {
      if (Conn *conn = handle_accept(fd)) {
        // put into the map, extend vector if idx out of bounds
        if (fd2conn.size() <= (size_t)conn->fd) {
          fd2conn.resize((size_t)conn->fd + 1);
        }
        fd2conn[conn->fd] = conn;
      }
    }

    for (size_t i{1}; i < poll_args.size(); ++i) {
      uint32_t ready = poll_args[i].revents;
      Conn *conn = fd2conn[poll_args[i].fd];

      // read and write

      if (ready & POLLIN) {
        handle_read(conn); // application logic
      }
      if (ready & POLLOUT) {
        handle_write(conn); // application logic
      }

      if ((ready & POLLERR) || conn->want_close) {
        (void)close(conn->fd); // callback could be added here like handle_err
        fd2conn[conn->fd] = nullptr;
        delete conn;
      }
    }
  }

  return 0;
}

static void fd_set_nb(int fd) {
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

static Conn *handle_accept(int fd) {
  // new connection fd by accepting on listening socket fd
  struct sockaddr_in client_addr = {};
  socklen_t addrlen = sizeof(client_addr);
  int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
  if (connfd < 0) {
    msg("accept() error");
    return nullptr;
  }

  uint32_t ip = client_addr.sin_addr.s_addr;
    fprintf(stderr, "new client from %u.%u.%u.%u:%u\n",
        ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24,
        ntohs(client_addr.sin_port)
  );

  // set the new connection fd to non-blocking mode
  fd_set_nb(connfd);
  return new Conn{
      .fd = connfd, .want_read = true, .incoming = {}, .outgoing = {}};
}

static void handle_read(Conn *conn) {
  // Do a non blocking read
  uint8_t buf[64 * 1024];
  ssize_t rv = read(conn->fd, buf, sizeof(buf));
  if (rv <= 0) { // error or EOF
    conn->want_close = true;
    return;
  }

  conn->incoming.append(buf, (size_t)rv);
  while (try_one_request(conn))
    ; // process all requests in the buffer (pipelining)

  if (!conn->outgoing.empty()) { // connection changes here because i think if
                                 // something is inside outgoing that means we
                                 // now need to write to connection
    conn->want_write = true;
    conn->want_read = false;
    return handle_write(conn); // optimisation: avoid another loop of poll()
  }
}

static void handle_write(Conn *conn) {
  assert(!conn->outgoing.empty());
  ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());
  // check if kernel buffer overwhelmed because writes coming from read now
  if (rv < 0 && errno == EAGAIN) {
    return;
  }

  if (rv < 0) {
    conn->want_close = true;
    return;
  }
  conn->outgoing.consume((size_t)rv);

  if (conn->outgoing.empty()) { // ditto in read
    conn->want_read = true;
    conn->want_write = false;
  }
}

// Protocol: length-prefixed messages
static bool try_one_request(Conn *conn) {
  if (conn->incoming.size() < 4) {
    return false;
  }
  uint32_t len = 0;
  memcpy(&len, conn->incoming.data(), 4);
  if (len > k_max_msg) { // error, exceed max size
    conn->want_close = true;
    return false;
  }

  if (4 + len > conn->incoming.size()) {
    return false; // this means read (weird)
  }

  const uint8_t *request = conn->incoming.data() + 4;

  std::vector<std::string> cmd;
  if (parse_req(request, len, cmd) < 0) {
    conn->want_close = true;
    return false;
  }

  size_t header_pos = 0;
  response_begin(conn->outgoing, &header_pos);
  do_request_with_hm(cmd, conn->outgoing);
  response_end(conn->outgoing, header_pos);

  conn->incoming.consume(
      4 + len); // note: important to consume and not clear the whole
                // buffer because there could be pipelined requests

  return true;
}

static int32_t parse_req(const uint8_t *data, size_t size,
                         std::vector<std::string> &out) {
  const uint8_t *end = data + size;
  uint32_t nstr = 0;

  // we know first 4 bytes is num args
  if (!read_u32(data, end, nstr)) {
    return -1; // error
  }
  if (nstr > k_max_args) {
    return -1; // error
  }

  // populate vector with requests (nstr)
  while (out.size() < nstr) {
    // first gives length of string
    uint32_t len = 0;
    if (!read_u32(data, end, len)) {
      return -1; // error
    }
    // push empty string into out and read into it
    out.push_back(std::string());
    if (!read_str(data, end, len, out.back())) {
      return -1; // error
    }
  }

  if (data != end) {
    return -1; // error, extra bytes
  }

  return 0; // success
}

static bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out) {
  if (cur + 4 > end) {
    return false;
  }
  memcpy(&out, cur, 4);
  cur += 4;
  return true;
}

static bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n,
                     std::string &out) {
  if (cur + n > end) {
    return false;
  }
  out.assign(cur, cur + n);
  cur += n;
  return true;
}

// APPLICATION LOGIC <- the db part
static void do_request_with_hm(std::vector<std::string> &cmd, Buffer &out) {
  if (cmd.size() == 2 && cmd[0] == "GET") {
    return do_get(cmd, out);
  } else if (cmd.size() == 3 && cmd[0] == "SET") {
    return do_set(cmd, out);
  } else if (cmd.size() == 2 && cmd[0] == "DEL") {
    return do_del(cmd, out);
  } else {
    return out_err(out, ERR_UNKNOWN, "unknown command.");
  }
}

static void do_get(std::vector<std::string> &cmd, Buffer &out) {
  // create key for lookup
  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
  HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
  if (!node) {
    return out_nil(out);
  }
  const std::string &val = container_of(node, Entry, node)->val;
  assert(val.size() <= k_max_msg);
  return out_str(out, val.data(), val.size());
}

static void do_set(std::vector<std::string> &cmd, Buffer &out) {
  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
  HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
  if (node) {
    container_of(node, struct Entry, node)->val.swap(cmd[2]);
  } else {
    // not found, allocate & insert a new pair
    Entry *ent = new Entry();
    ent->key.swap(key.key);
    ent->node.hcode = key.node.hcode;
    ent->val.swap(cmd[2]);
    hm_insert(&g_data.db, &ent->node);
  }
  return out_nil(out);
}

static void do_del(std::vector<std::string> &cmd, Buffer &out) {
  // a dummy entry for the lookup
  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
  HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
  hm_delete(&g_data.db, &key.node, &entry_eq);
  if (node) {
    delete container_of(node, Entry, node);
  }
  return out_int(out, node ? 1 : 0);
}

static bool entry_eq(HNode *lhs, HNode *rhs) {
  struct Entry *le = container_of(lhs, struct Entry, node);
  struct Entry *re = container_of(rhs, struct Entry, node);
  return le->key == re->key;
}

// FNV hash
static uint64_t str_hash(const uint8_t *data, size_t len) {
  uint32_t h = 0x811C9DC5;
  for (size_t i = 0; i < len; i++) {
    h = (h + data[i]) * 0x01000193;
  }
  return h;
}