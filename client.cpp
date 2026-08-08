#include "shared.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int32_t query(int fd, const char *text);

int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("socket()");
  }

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(k_port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv) {
    die("connect");
  }

  int32_t err = query(fd, "hello");
  if (err) {
    close(fd);
    return 0;
  }

  err = query(fd, "hello2");

  close(fd);
  return 0;
}

static int32_t query(int fd, const char *text) {
  uint32_t len = (uint32_t)strlen(text);
  if (len > k_max_msg) {
    return -1;
  }
  // send request
  char wbuf[4 + k_max_msg];
  memcpy(wbuf, &len, 4); // assume little endian
  memcpy(&wbuf[4], text, len);
  if (int32_t err = write_fully(fd, wbuf, 4 + len)) {
    return err;
  }
  // 4 bytes header
  char rbuf[4 + k_max_msg];
  errno = 0;
  int32_t err = read_fully(fd, rbuf, 4);
  if (err) {
    msg_error(errno == 0 ? "EOF" : "read() error");
    return err;
  }
  memcpy(&len, rbuf, 4); // assume little endian
  if (len > k_max_msg) {
    msg_error("too long");
    return -1;
  }
  // reply body
  err = read_fully(fd, &rbuf[4], len);
  if (err) {
    msg_error("read() error");
    return err;
  }
  // do something
  printf("server says: %.*s\n", (int)len, &rbuf[4]);
  return 0;
}