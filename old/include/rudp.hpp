#pragma once

#include <sys/socket.h>
#include <sys/types.h>

namespace rudp {

int socket(void);
int bind(int sockfd, struct sockaddr* addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
int connect(int sockfd, struct sockaddr* addr, socklen_t addrlen);
size_t send(int sockfd, const void* buf, size_t len, int flags);
size_t recv(int sockfd, void* buf, size_t len, int flags);
int close(int sockfd);

}  // namespace rudp
