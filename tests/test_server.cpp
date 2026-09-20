#include "protocol.hpp"
#include "shared.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <cstdio>
#include <csignal>
#include <fcntl.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

// todo: put this in a config file or something
#ifndef SERVER_PATH
#define SERVER_PATH "./server"
#endif

static int g_pass = 0;
static int g_fail = 0;

static void report(const char *name, bool ok, const std::string &detail = "") {
  if (ok) {
    printf("[OK]   %s\n", name);
    ++g_pass;
  } else {
    printf("[FAIL] %s%s%s\n", name, detail.empty() ? "" : " -- ", detail.c_str());
    ++g_fail;
  }
}

static std::string describe(const Value &v) {
  switch (v.tag) {
  case TAG_NIL:
    return "nil";
  case TAG_ERR:
    return "err " + std::to_string(v.err_code) + " \"" + v.err_msg + "\"";
  case TAG_STR:
    return "str \"" + v.str + "\" (len " + std::to_string(v.str.size()) + ")";
  case TAG_INT:
    return "int " + std::to_string(v.i64);
  case TAG_DBL:
    return "dbl " + std::to_string(v.dbl);
  case TAG_ARR:
    return "arr len=" + std::to_string(v.arr.size());
  default:
    return "?(tag=" + std::to_string(v.tag) + ")";
  }
}

// ---- server subprocess management ----

class ServerProcess {
public:
  ServerProcess() {
    pid_ = fork();
    if (pid_ < 0) {
      die("fork() failed");
    }
    if (pid_ == 0) {
      // child: keep the server's own stdout/stderr out of the test output
      int devnull = open("/dev/null", O_WRONLY);
      if (devnull >= 0) {
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        close(devnull);
      }
      execl(SERVER_PATH, SERVER_PATH, (char *)nullptr);
      _exit(127); // exec failed
    }
    wait_until_ready();
  }

  ~ServerProcess() {
    if (pid_ > 0) {
      kill(pid_, SIGTERM);
      waitpid(pid_, nullptr, 0);
    }
  }

  ServerProcess(const ServerProcess &) = delete;
  ServerProcess &operator=(const ServerProcess &) = delete;

private:
  void wait_until_ready() {
    for (int attempt = 0; attempt < 100; ++attempt) {
      int fd = socket(AF_INET, SOCK_STREAM, 0);
      struct sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(k_port);
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        close(fd);
        return;
      }
      close(fd);
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    die("server did not become ready in time");
  }

  pid_t pid_{-1};
};

static int connect_to_server() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("socket()");
  }
  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(k_port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    die("connect()");
  }
  return fd;
}

// one-shot request/response over a fresh connection, mirroring client.cpp
static Value do_cmd(const std::vector<std::string> &cmd) {
  int fd = connect_to_server();
  Value v;
  if (send_request(fd, cmd) != 0 || !read_response(fd, v)) {
    v = Value{};
    v.tag = 0xFF; // sentinel: transport failure, not a real protocol tag
  }
  close(fd);
  return v;
}

// ---- test cases ----

static void test_get_missing_key() {
  Value v = do_cmd({"GET", "no-such-key-xyz"});
  report("GET on a missing key returns nil", v.tag == TAG_NIL, "got " + describe(v));
}

static void test_set_then_get() {
  Value setv = do_cmd({"SET", "k1", "hello"});
  report("SET returns nil", setv.tag == TAG_NIL, "got " + describe(setv));

  Value getv = do_cmd({"GET", "k1"});
  report("GET after SET returns the stored value", getv.tag == TAG_STR && getv.str == "hello",
         "got " + describe(getv));
}

static void test_overwrite_existing_key() {
  do_cmd({"SET", "k1", "first"});
  do_cmd({"SET", "k1", "second"});
  Value v = do_cmd({"GET", "k1"});
  report("SET overwrites an existing key", v.tag == TAG_STR && v.str == "second", "got " + describe(v));
}

static void test_del_existing_key() {
  do_cmd({"SET", "k2", "bye"});
  Value delv = do_cmd({"DEL", "k2"});
  report("DEL on an existing key returns int 1", delv.tag == TAG_INT && delv.i64 == 1,
         "got " + describe(delv));

  Value getv = do_cmd({"GET", "k2"});
  report("GET after DEL returns nil", getv.tag == TAG_NIL, "got " + describe(getv));
}

static void test_del_missing_key() {
  Value v = do_cmd({"DEL", "never-existed-xyz"});
  report("DEL on a missing key returns int 0", v.tag == TAG_INT && v.i64 == 0, "got " + describe(v));
}

static void test_unknown_command() {
  Value v = do_cmd({"FROBNICATE", "x"});
  report("an unknown command returns an error", v.tag == TAG_ERR, "got " + describe(v));
}

static void test_empty_value_roundtrip() {
  do_cmd({"SET", "empty", ""});
  Value v = do_cmd({"GET", "empty"});
  report("SET/GET round-trips an empty value", v.tag == TAG_STR && v.str.empty(), "got " + describe(v));
}

static void test_large_value_roundtrip() {
  std::string big(4000, 'x');
  do_cmd({"SET", "big", big});
  Value v = do_cmd({"GET", "big"});
  report("SET/GET round-trips a 4000-byte value (forces Buffer growth)",
         v.tag == TAG_STR && v.str == big, "got " + describe(v));
}

static void test_pipelined_requests() {
  int fd = connect_to_server();

  std::vector<uint8_t> wire;
  auto push_cmd = [&](std::vector<std::string> cmd) {
    std::vector<uint8_t> bytes = encode_request(cmd);
    wire.insert(wire.end(), bytes.begin(), bytes.end());
  };
  push_cmd({"SET", "pa", "1"});
  push_cmd({"SET", "pb", "2"});
  push_cmd({"GET", "pa"});
  push_cmd({"GET", "pb"});
  push_cmd({"DEL", "pa"});
  push_cmd({"GET", "pa"});

  bool ok = write_all(fd, (const char *)wire.data(), wire.size()) == 0;

  Value v;
  ok = ok && read_response(fd, v) && v.tag == TAG_NIL;                       // SET pa
  ok = ok && read_response(fd, v) && v.tag == TAG_NIL;                       // SET pb
  ok = ok && read_response(fd, v) && v.tag == TAG_STR && v.str == "1";       // GET pa
  ok = ok && read_response(fd, v) && v.tag == TAG_STR && v.str == "2";       // GET pb
  ok = ok && read_response(fd, v) && v.tag == TAG_INT && v.i64 == 1;         // DEL pa
  ok = ok && read_response(fd, v) && v.tag == TAG_NIL;                       // GET pa (gone)

  report("6 pipelined requests on one connection all round-trip in order", ok);
  close(fd);
}

static void test_partial_read() {
  int fd = connect_to_server();
  std::vector<uint8_t> wire = encode_request({"SET", "partial", "hello-world"});

  size_t split = wire.size() / 2;
  bool ok = write_all(fd, (const char *)wire.data(), split) == 0;
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  ok = ok && write_all(fd, (const char *)wire.data() + split, wire.size() - split) == 0;

  Value v;
  ok = ok && read_response(fd, v) && v.tag == TAG_NIL;
  close(fd);

  Value getv = do_cmd({"GET", "partial"});
  ok = ok && getv.tag == TAG_STR && getv.str == "hello-world";

  report("a request split across two separate writes is still parsed correctly", ok);
}

int main() {
  printf("Running my-redis integration tests (%s)...\n\n", SERVER_PATH);

  ServerProcess server;

  test_get_missing_key();
  test_set_then_get();
  test_overwrite_existing_key();
  test_del_existing_key();
  test_del_missing_key();
  test_unknown_command();
  test_empty_value_roundtrip();
  test_large_value_roundtrip();
  test_pipelined_requests();
  test_partial_read();

  printf("\n%d passed, %d failed, %d total\n", g_pass, g_fail, g_pass + g_fail);
  return g_fail == 0 ? 0 : 1;
}
