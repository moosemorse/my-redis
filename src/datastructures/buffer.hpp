#pragma once

#include <stddef.h>
#include <stdint.h>

class Buffer {
public:
  Buffer();
  explicit Buffer(size_t initial_capacity);
  ~Buffer();

  Buffer(const Buffer &) = delete;
  Buffer &operator=(const Buffer &) = delete;
  Buffer(Buffer &&) = delete;
  Buffer &operator=(Buffer &&) = delete;

  void append(const uint8_t *data, size_t len);
  void consume(size_t n);
  void truncate(size_t new_size); // shrink to new_size bytes, discarding the tail

  const uint8_t *data() const { return data_begin_; }
  uint8_t *mutable_data() { return data_begin_; }
  size_t size() const { return static_cast<size_t>(data_end_ - data_begin_); }
  bool empty() const { return data_begin_ == data_end_; }

private:
  size_t capacity() const { return static_cast<size_t>(buffer_end_ - buffer_begin_); }
  size_t occupied() const { return static_cast<size_t>(data_end_ - data_begin_); }
  size_t remaining_at_end() const { return static_cast<size_t>(buffer_end_ - data_end_); }

  void compact();
  void grow(size_t min_extra);

  uint8_t *buffer_begin_{nullptr};
  uint8_t *buffer_end_{nullptr};
  uint8_t *data_begin_{nullptr};
  uint8_t *data_end_{nullptr};
};
