#include "shared.h"

#include <assert.h>
#include <errno.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void msg_error(std::string_view err) { std::cerr << err << std::endl; }

void die(const char *msg) {
  int err = errno;
  fprintf(stderr, "[%d] %s\n", err, msg);
  abort();
}

int32_t read_fully(int fd, char *buf, size_t n) {
  while (n > 0) {
    ssize_t bytes = read(fd, buf, n);
    if (bytes < 0 && errno == EINTR) {
      errno = 0;
      continue;
    }
    if (bytes <= 0)
      return -1;
    assert((size_t)bytes <= n);
    n -= (size_t)bytes;
    buf += bytes;
  }

  return 0;
}

int32_t write_fully(int fd, const char *buf, size_t n) {
  while (n > 0) {
    ssize_t bytes = write(fd, buf, n);
    if (bytes < 0 && errno == EINTR) {
      errno = 0;
      continue;
    }
    if (bytes <= 0)
      return -1;
    assert((size_t)bytes <= n);
    n -= (size_t)bytes;
    buf += bytes;
  }

  return 0;
}
