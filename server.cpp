/*
pseudocode for a simple TCP server:

fd = socket() <--- (1)
bind(fd, address)
listen(fd)
while (true)
{
    conn_fd = accept(fd) <-- accept client connection
    do_something_with(conn_fd) <-- handle the client connection is that like
sending stuff over?? close(conn_fd) <-- close for resource management
}
*/

#include "shared.h"

#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int32_t process_client(int connfd);

int main() {
  /// (1) creating and configure socket

  int fd = socket(AF_INET, SOCK_STREAM, 0);

  int val{1};
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  // (2) bind socket to address

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(k_port);
  addr.sin_addr.s_addr = htonl(0);
  if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr))) {
    throw std::runtime_error("bind() error");
  }

  // (3) listen for incoming connections

  if (listen(fd, SOMAXCONN)) {
    throw std::runtime_error("listen() error");
  }

  // (4) accept connections and process

  while (true) {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
    if (connfd < 0) {
      msg_error("connection failed with client");
      continue;
    }

    while (true) {
      if (process_client(connfd)) {
        break;
      }
    }

    close(connfd);
  }

  return 0;
}

static int32_t process_client(int connfd) {
  // 4 bytes header
  char rbuf[4 + k_max_msg];
  errno = 0; // by default errno is prev value, so set to 0
  int32_t err = read_fully(connfd, rbuf, 4);
  if (err) {
    msg_error(errno == 0 ? "EOF" : "read() error");
    return err;
  }
  uint32_t len = 0;
  memcpy(&len, rbuf, 4); // assume little endian
  if (len > k_max_msg) {
    msg_error("too long");
    return -1;
  }

  // request body
  err = read_fully(connfd, &rbuf[4], len);
  if (err) {
    msg_error("read() error");
    return err;
  }

  // do something
  printf("client says: %.*s\n", (int)len, &rbuf[4]);

  // reply using the same protocol
  const char reply[] = "world";
  char wbuf[4 + sizeof(reply)];
  len = (uint32_t)strlen(reply);
  memcpy(wbuf, &len, 4);
  memcpy(&wbuf[4], reply, len);
  return write_fully(connfd, wbuf, 4 + len);
}