#pragma once

#include <sys/socket.h>
#include <sys/types.h>

int rudp_socket(void);
int rudp_bind(int sockfd, struct sockaddr* addr, socklen_t addrlen);
int rudp_connect(int sockfd, struct sockaddr* addr, socklen_t addrlen);
int rudp_listen(int sockfd, int backlog);
int rudp_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
size_t rudp_send(int sockfd, const void* buf, size_t len, int flags);
size_t rudp_recv(int sockfd, void* buf, size_t len, int flags);
int rudp_close(int sockfd);
