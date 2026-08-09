#pragma once

#include <stddef.h>
#include <stdint.h>

struct Buffer {
  uint8_t *buffer_begin;
  uint8_t *buffer_end;
  uint8_t *data_begin;
  uint8_t *data_end;
};

void buf_consume(struct Buffer *buf, size_t n);
void buf_append(struct Buffer *buf, const uint8_t *data, size_t len);