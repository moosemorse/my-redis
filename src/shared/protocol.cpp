#include "protocol.hpp"
#include "shared.hpp"

#include <cstring>
#include <cstdio>
#include <unistd.h>

int32_t read_full(int fd, char *buf, size_t n) {
  while (n > 0) {
    ssize_t rv = read(fd, buf, n);
    if (rv <= 0) {
      return -1; // error, or unexpected EOF
    }
    n -= (size_t)rv;
    buf += rv;
  }
  return 0;
}

int32_t write_all(int fd, const char *buf, size_t n) {
  while (n > 0) {
    ssize_t rv = write(fd, buf, n);
    if (rv <= 0) {
      return -1; // error
    }
    n -= (size_t)rv;
    buf += rv;
  }
  return 0;
}

std::vector<uint8_t> encode_request(const std::vector<std::string> &cmd) {
  uint32_t body_len = 4;
  for (const std::string &s : cmd) {
    body_len += 4 + (uint32_t)s.size();
  }

  std::vector<uint8_t> out;
  out.reserve(4 + body_len);

  auto append_u32 = [&out](uint32_t v) {
    const uint8_t *p = (const uint8_t *)&v;
    out.insert(out.end(), p, p + 4);
  };

  append_u32(body_len);
  append_u32((uint32_t)cmd.size());
  for (const std::string &s : cmd) {
    append_u32((uint32_t)s.size());
    out.insert(out.end(), s.begin(), s.end());
  }
  return out;
}

int32_t send_request(int fd, const std::vector<std::string> &cmd) {
  std::vector<uint8_t> wire = encode_request(cmd);
  if (wire.size() > 4 + k_max_msg) {
    return -1;
  }
  return write_all(fd, (const char *)wire.data(), wire.size());
}

static int32_t parse_value(const uint8_t *data, size_t size, Value &out) {
  if (size < 1) {
    return -1;
  }
  out = Value{};
  out.tag = data[0];

  switch (data[0]) {
  case TAG_NIL:
    return 1;

  case TAG_ERR: {
    if (size < 1 + 8) {
      return -1;
    }
    uint32_t code = 0, len = 0;
    memcpy(&code, &data[1], 4);
    memcpy(&len, &data[1 + 4], 4);
    if (size < 1 + 8 + len) {
      return -1;
    }
    out.err_code = code;
    out.err_msg.assign((const char *)&data[1 + 8], len);
    return (int32_t)(1 + 8 + len);
  }

  case TAG_STR: {
    if (size < 1 + 4) {
      return -1;
    }
    uint32_t len = 0;
    memcpy(&len, &data[1], 4);
    if (size < 1 + 4 + len) {
      return -1;
    }
    out.str.assign((const char *)&data[1 + 4], len);
    return (int32_t)(1 + 4 + len);
  }

  case TAG_INT: {
    if (size < 1 + 8) {
      return -1;
    }
    int64_t val = 0;
    memcpy(&val, &data[1], 8);
    out.i64 = val;
    return 1 + 8;
  }

  case TAG_DBL: {
    if (size < 1 + 8) {
      return -1;
    }
    double val = 0;
    memcpy(&val, &data[1], 8);
    out.dbl = val;
    return 1 + 8;
  }

  case TAG_ARR: {
    if (size < 1 + 4) {
      return -1;
    }
    uint32_t n = 0;
    memcpy(&n, &data[1], 4);
    size_t consumed = 1 + 4;
    out.arr.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
      Value elem;
      int32_t rv = parse_value(&data[consumed], size - consumed, elem);
      if (rv < 0) {
        return -1;
      }
      out.arr.push_back(std::move(elem));
      consumed += (size_t)rv;
    }
    return (int32_t)consumed;
  }

  default:
    return -1;
  }
}

bool read_response(int fd, Value &out) {
  char header[4];
  if (read_full(fd, header, 4) != 0) {
    return false;
  }
  uint32_t len = 0;
  memcpy(&len, header, 4);
  if (len > k_max_msg) {
    return false;
  }

  std::vector<char> body(len);
  if (read_full(fd, body.data(), len) != 0) {
    return false;
  }

  int32_t rv = parse_value((const uint8_t *)body.data(), len, out);
  return rv > 0 && (uint32_t)rv == len;
}

void print_value(const Value &v) {
  switch (v.tag) {
  case TAG_NIL:
    printf("(nil)\n");
    break;
  case TAG_ERR:
    printf("(err) %u %s\n", v.err_code, v.err_msg.c_str());
    break;
  case TAG_STR:
    printf("(str) %s\n", v.str.c_str());
    break;
  case TAG_INT:
    printf("(int) %lld\n", (long long)v.i64);
    break;
  case TAG_DBL:
    printf("(dbl) %g\n", v.dbl);
    break;
  case TAG_ARR:
    printf("(arr) len=%zu\n", v.arr.size());
    for (const Value &e : v.arr) {
      print_value(e);
    }
    printf("(arr) end\n");
    break;
  }
}
