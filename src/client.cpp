#include "protocol.hpp"
#include "shared.hpp"
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

int main(int argc, char **argv) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(k_port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // 127.0.0.1
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("connect");
    }

    std::vector<std::string> cmd;
    for (int i = 1; i < argc; ++i) {
        cmd.push_back(argv[i]);
    }

    if (send_request(fd, cmd) != 0) {
        msg("failed to send request");
        close(fd);
        return 1;
    }

    Value resp;
    if (!read_response(fd, resp)) {
        msg("bad response");
        close(fd);
        return 1;
    }
    print_value(resp);

    close(fd);
    return 0;
}
