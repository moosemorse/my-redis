#pragma once

#include <cstdint>
#include <string>
#include <vector>

// wire protocol shared by client.cpp and the integration tests: a 4-byte
// length-prefixed request/response framing, with responses tagged by type.

enum Tag : uint8_t {
  TAG_NIL = 0, // nil
  TAG_ERR = 1, // error code + msg
  TAG_STR = 2, // string
  TAG_INT = 3, // int64
  TAG_DBL = 4, // double
  TAG_ARR = 5, // array
};

struct Value {
  uint8_t tag{TAG_NIL};
  std::string str;        // TAG_STR
  int64_t i64{0};          // TAG_INT
  double dbl{0};            // TAG_DBL
  uint32_t err_code{0};     // TAG_ERR
  std::string err_msg;      // TAG_ERR
  std::vector<Value> arr;   // TAG_ARR
};

// retry until exactly n bytes are moved, or an error/EOF occurs (returns -1)
int32_t read_full(int fd, char *buf, size_t n);
int32_t write_all(int fd, const char *buf, size_t n);

// pure encoding: build the request's wire bytes, no I/O
std::vector<uint8_t> encode_request(const std::vector<std::string> &cmd);

// encode + write a request in one call
int32_t send_request(int fd, const std::vector<std::string> &cmd);

// read exactly one framed response from fd and parse it
bool read_response(int fd, Value &out);

// human-readable rendering, used by the CLI client
void print_value(const Value &v);
