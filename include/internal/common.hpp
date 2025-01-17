#pragma once

#include <sys/epoll.h>
#include <sys/socket.h>

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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

namespace rudp {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

using f32 = float;
using f64 = double;

using b8 = bool;

// TODO: Consider new names for linuxfd_t as it's not a linux-specific concept.
using linuxfd_t = s32;
using rudpfd_t = s32;

// TODO: Reconsider this organisation.
namespace internal {
    namespace constants {
        inline constexpr s32 UNINITIALISED_FD = -1;
        inline constexpr u32 PHI = 0x9e3779b9;
    }  // namespace constants

    template <typename F>
    void preserve_errno(F cleanup) {
        int saved_errno = errno;
        cleanup();
        errno = saved_errno;
    }

    inline bool is_valid_sockfd(linuxfd_t fd) {
        int opt;
        socklen_t opt_len = sizeof(opt);
        return getsockopt(fd, SOL_SOCKET, SO_TYPE, &opt, &opt_len) != -1;
    }

    inline bool is_valid_epollfd(linuxfd_t fd) {
        return epoll_wait(fd, NULL, 0, 0) != -1 || errno == EINTR;
    }
}  // namespace internal

RUDP_STATIC_ASSERT(internal::constants::UNINITIALISED_FD < 0, "Uninitialised FD must be negative.");

}  // namespace rudp
