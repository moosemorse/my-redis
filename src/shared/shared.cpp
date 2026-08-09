#include "shared.hpp"

#include <assert.h>
#include <errno.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void msg(std::string_view err) { std::cerr << err << std::endl; }

void die(const char *msg) {
  int err = errno;
  fprintf(stderr, "[%d] %s\n", err, msg);
  abort();
}
