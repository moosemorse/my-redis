#pragma once

#include <cassert>
#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include <vector>

const size_t k_max_msg = 4096;
const uint16_t k_port = 1234;

void msg_error(std::string_view err);
[[noreturn]] void die(const char *msg);
int32_t read_fully(int fd, char *buf, size_t n);
int32_t write_fully(int fd, const char *buf, size_t n);

inline std::string_view msg_view(const std::vector<uint8_t> &buf, size_t off,
                                 size_t n) {
  assert(off + n <= buf.size());
  return {reinterpret_cast<const char *>(buf.data() + off), n};
}
