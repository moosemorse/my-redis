#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string_view>

const size_t k_max_msg = 4096;
const uint16_t k_port = 1234;

void msg_error(std::string_view err);
[[noreturn]] void die(const char *msg);
int32_t read_fully(int fd, char *buf, size_t n);
int32_t write_fully(int fd, const char *buf, size_t n);
