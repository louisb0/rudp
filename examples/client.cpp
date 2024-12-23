#include <arpa/inet.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <rudp.hpp>

int main() {
    int fd = rudp_socket();
    if (fd < 0) {
        perror("rudp_socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);

    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if (rudp_connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) <= 0) {
        perror("rudp_connect");
        exit(EXIT_FAILURE);
    }

    printf("Connected\n");

    rudp_close(fd);
    return 0;
}
