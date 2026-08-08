/*
pseudocode for a simple TCP server:

fd = socket() <--- (1) 
bind(fd, address)
listen(fd)
while (true) 
{
    conn_fd = accept(fd) <-- accept client connection
    do_something_with(conn_fd) <-- handle the client connection is that like sending stuff over??
    close(conn_fd) <-- close for resource management
}
*/

#include <stdlib.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void process_client(int connfd);
static void msg_error(std::string_view err);

int main()
{
    /// (1) creating and configure socket

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    int val {1};
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // (2) bind socket to address

    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(0);
    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));

    // (3) listen for incoming connections

    rv = listen(fd, SOMAXCONN);
    if (rv) { throw std::runtime_error("listen() error"); }

    // (4) accept connections and process

    while (true)
    {
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
        if (connfd < 0)
        {
            msg_error("connection failed with client");
            continue;
        }

        process_client(connfd);
        close(connfd);
    }

    return 0;
}

static void process_client(int connfd) 
{
    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0)
    {
        msg_error("read() error");
        return;
    }

    std::cout << "received: " << rbuf << std::endl;

    char wbuf[] = "world";
    auto bytes = write(connfd, wbuf, strlen(wbuf)); // some c code
    if (bytes < 0)
    {
        msg_error("write() error");
        return;
    }
}

static void msg_error(std::string_view err)
{
    std::cerr << err << std::endl;
}