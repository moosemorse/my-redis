#pragma once

#include <cassert>
#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include <vector>

const size_t k_max_msg = 4096;
const uint16_t k_port = 1234;

void msg(std::string_view err);
[[noreturn]] void die(const char *msg);

inline std::string_view msg_view(const std::vector<uint8_t> &buf, size_t off,
                                 size_t n) {
  assert(off + n <= buf.size());
  return {reinterpret_cast<const char *>(buf.data() + off), n};
}
