#include <arpa/inet.h>
#include <sys/socket.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <rudp.hpp>

int main() {
    int server_fd = rudp_socket();
    if (server_fd < 0) {
        perror("rudp_socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8888);

    if (rudp_bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("rudp_bind");
        exit(EXIT_FAILURE);
    }

    printf("Listening...\n");

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = rudp_accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("rudp_accept");
            continue;
        }

        printf("Connected to %s:%d\n", inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));
        rudp_close(client_fd);
    }
}
