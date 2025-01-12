#include <sys/socket.h>
#include <sys/types.h>

#include "internal/common.hpp"
#include "internal/socket.hpp"

namespace rudp {

int socket(void) {
    rudpfd_t fd = internal::next_fd++;
    internal::g_sockets.emplace(fd);
    return fd;
}

int bind(int sockfd, struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int connect(int sockfd, struct sockaddr *addr, socklen_t addrlen);
size_t send(int sockfd, const void *buf, size_t len, int flags);
size_t recv(int sockfd, void *buf, size_t len, int flags);
int close(int sockfd);

}  // namespace rudp
