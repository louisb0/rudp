#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <rudp.hpp>

int main() {
    int server_fd = rudp::socket();
    if (server_fd < 0) {
        perror("rudp::socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8888);

    if (rudp::bind(server_fd, reinterpret_cast<struct sockaddr *>(&server_addr),
                   sizeof(server_addr)) < 0) {
        perror("rudp::bind");
        exit(EXIT_FAILURE);
    }

    if (rudp::listen(server_fd, 5) < 0) {
        perror("rudp::listen");
        exit(EXIT_FAILURE);
    }

    printf("Listening...\n");

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd =
            rudp::accept(server_fd, reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);
        if (client_fd < 0) {
            perror("rudp::accept");
            continue;
        }

        printf("Connected to %s:%d\n", inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        char buf[2049];
        ssize_t bytes = rudp::recv(client_fd, buf, 2049, 0);
        if (bytes < 0) {
            perror("rudp::recv");
        } else {
            buf[bytes] = '\0';
            printf("%s[END]\n", buf);
        }

        // rudp::close(client_fd);
    }
}
