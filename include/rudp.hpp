#pragma once

#include <sys/socket.h>
#include <sys/types.h>

namespace rudp {

[[nodiscard]] int socket(void) noexcept;
[[nodiscard]] int bind(int sockfd, struct sockaddr *addr, socklen_t addrlen) noexcept;
[[nodiscard]] int listen(int sockfd, int backlog) noexcept;
[[nodiscard]] int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) noexcept;
[[nodiscard]] int connect(int sockfd, struct sockaddr *addr, socklen_t addrlen) noexcept;
[[nodiscard]] size_t send(int sockfd, const void *buf, size_t len, int flags) noexcept;
[[nodiscard]] size_t recv(int sockfd, void *buf, size_t len, int flags) noexcept;
int close(int sockfd) noexcept;

}  // namespace rudp
