#include "protocol.hpp"
#include "shared.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <netinet/ip.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// client side connection
static int connect_local(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("socket()");
  }
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(fd, (const struct sockaddr *)&addr, sizeof(addr))) {
    die("connect");
  }
  return fd;
}

int main(int argc, char **argv) {
  int port = argc > 1 ? atoi(argv[1]) : k_port;
  int n = argc > 2 ? atoi(argv[2]) : 100000;
  constexpr int kKeyspace = 10000;

  int fd = connect_local(port);
  Value resp;

  auto t0 = std::chrono::steady_clock::now();
  // 0..n set requests
  for (int i = 0; i < n; ++i) {
    std::string key = "key" + std::to_string(i % kKeyspace);
    if (send_request(fd, {"SET", key, "somevalue1234567890"}) != 0) {
      die("send_request");
    }
    if (!read_response(fd, resp)) {
      die("read_response");
    }
  }
  auto t1 = std::chrono::steady_clock::now();
  double set_secs = std::chrono::duration<double>(t1 - t0).count();
  printf("SET: %d ops in %.3fs -> %.0f ops/sec\n", n, set_secs, n / set_secs);

  t0 = std::chrono::steady_clock::now();
  // 0..n get requests timed for valid keys
  for (int i = 0; i < n; ++i) {
    std::string key = "key" + std::to_string(i % kKeyspace);
    if (send_request(fd, {"GET", key}) != 0) {
      die("send_request");
    }
    if (!read_response(fd, resp)) {
      die("read_response");
    }
  }
  t1 = std::chrono::steady_clock::now();
  double get_secs = std::chrono::duration<double>(t1 - t0).count();
  printf("GET: %d ops in %.3fs -> %.0f ops/sec\n", n, get_secs, n / get_secs);

  close(fd);
  return 0;
}
