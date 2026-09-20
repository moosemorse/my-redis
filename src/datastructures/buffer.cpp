#include "buffer.hpp"

#include <assert.h>
#include <string.h>

Buffer::Buffer() : Buffer(4096) {}

Buffer::Buffer(size_t initial_capacity) {
  size_t cap = initial_capacity ? initial_capacity : 64;
  buffer_begin_ = new uint8_t[cap];
  buffer_end_ = buffer_begin_ + cap;
  data_begin_ = buffer_begin_;
  data_end_ = buffer_begin_;
}

Buffer::~Buffer() { delete[] buffer_begin_; }

void Buffer::consume(size_t n) {
  assert(data_begin_ + n <= data_end_);
  data_begin_ += n;
}

void Buffer::truncate(size_t new_size) {
  assert(new_size <= occupied());
  data_end_ = data_begin_ + new_size;
}

void Buffer::append(const uint8_t *data, size_t len) {
  if (len == 0) {
    return;
  }

  if (remaining_at_end() < len) {
    // total free space (already-consumed prefix + free tail) is enough:
    // shift the unconsumed bytes down to reclaim it, no allocation needed.
    if (capacity() - occupied() >= len) {
      compact();
    } else {
      grow(len);
    }
  }

  memcpy(data_end_, data, len);
  data_end_ += len;
}

void Buffer::compact() {
  if (data_begin_ == buffer_begin_) {
    return;
  }
  size_t n = occupied();
  memmove(buffer_begin_, data_begin_, n);
  data_begin_ = buffer_begin_;
  data_end_ = buffer_begin_ + n;
}

void Buffer::grow(size_t min_extra) {
  size_t occ = occupied();
  size_t needed = occ + min_extra;
  size_t new_cap = capacity() ? capacity() : 64;
  while (new_cap < needed) {
    new_cap *= 2;
  }

  uint8_t *new_buffer = new uint8_t[new_cap];
  memcpy(new_buffer, data_begin_, occ);
  delete[] buffer_begin_;

  buffer_begin_ = new_buffer;
  buffer_end_ = new_buffer + new_cap;
  data_begin_ = new_buffer;
  data_end_ = new_buffer + occ;
}
