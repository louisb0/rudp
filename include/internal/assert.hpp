#pragma once

#include <cerrno>
#include <cstddef>

#include "internal/common.hpp"

#define RUDP_ASSERT_1(condition)                                         \
    do {                                                                 \
        if (!(condition)) {                                              \
            fprintf(stderr, "Assertion failed: %s\n", #condition);       \
            fprintf(stderr, "File: %s, Line: %d\n", __FILE__, __LINE__); \
            abort();                                                     \
        }                                                                \
    } while (0)

#define RUDP_ASSERT_N(condition, ...)                                      \
    do {                                                                   \
        if (!(condition)) {                                                \
            fprintf(stderr, "Assertion failed: %s\n", #condition);         \
            fprintf(stderr, "Message: ");                                  \
            fprintf(stderr, __VA_ARGS__);                                  \
            fprintf(stderr, "\nFile: %s, Line: %d\n", __FILE__, __LINE__); \
            abort();                                                       \
        }                                                                  \
    } while (0)

#define _RUDP_GET_MACRO(_1, _2, _3, _4, _5, NAME, ...) NAME

#define RUDP_ASSERT(...)                                                                     \
    _RUDP_GET_MACRO(__VA_ARGS__, RUDP_ASSERT_N, RUDP_ASSERT_N, RUDP_ASSERT_N, RUDP_ASSERT_N, \
                    RUDP_ASSERT_1)(__VA_ARGS__)
#define RUDP_STATIC_ASSERT(condition, ...) static_assert(condition)
#define RUDP_UNREACHABLE() RUDP_ASSERT(false, "This point is unreachable.")

namespace rudp::internal {

inline bool is_valid_sockfd(linuxfd_t fd) {
    int opt;
    socklen_t opt_len = sizeof(opt);
    return getsockopt(fd, SOL_SOCKET, SO_TYPE, &opt, &opt_len) != -1;
}

inline bool is_valid_epollfd(linuxfd_t fd) {
    // NOTE: A bad epollfd returns EINVAL, whereas a bad fd (-1) returns EBADF.
    return epoll_ctl(fd, EPOLL_CTL_DEL, -1, NULL) == -1 && errno == EBADF;
}

}  // namespace rudp::internal
