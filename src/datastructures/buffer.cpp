#include "buffer.hpp"

#include <assert.h>
#include <string.h>

static size_t buffer_remaining_space(struct Buffer *buf);

// advance pointer
void buf_consume(struct Buffer *buf, size_t n) {
  assert(buf->data_begin + n <= buf->data_end);
  buf->data_begin += n;
}

void buf_append(struct Buffer *buf, const uint8_t *data, size_t len) {
  // check we have enough room, using fixed allocation buffer
  assert(buffer_remaining_space(buf) >= len);

  // move data to front to make room
  size_t space_occupied = buf->data_end - buf->data_begin;
  memmove(buf->buffer_begin, buf->data_begin, space_occupied);
  buf->data_begin = buf->buffer_begin;
  buf->data_end = buf->data_begin + space_occupied;

  // copy new data to the end of the buffer
  memcpy(buf->data_end, data, len);
  buf->data_end += len;
}

// assumption: buffer allocated with new[]
void buf_append_realloc(struct Buffer *buf, const uint8_t *data, size_t len) {
  // check we have enough room, if not allocate more space to buffer
  if (buffer_remaining_space(buf) < len) {
    size_t data_size = buf->data_end - buf->data_begin;
    size_t new_size = data_size + len;
    uint8_t *new_buffer = new uint8_t[new_size];
    memcpy(new_buffer, buf->data_begin, data_size);
    delete[] buf->buffer_begin;
    buf->buffer_begin = new_buffer;
    buf->buffer_end = new_buffer + new_size;
    buf->data_begin = new_buffer;
    buf->data_end = new_buffer + data_size;
  }
  memcpy(buf->data_end, data, len);
  buf->data_end += len;
}

static size_t buffer_remaining_space(struct Buffer *buf) {
  return (buf->buffer_end - buf->buffer_begin) -
         (buf->data_end - buf->data_begin);
}