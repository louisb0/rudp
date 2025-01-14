#pragma once

#include <sys/socket.h>

#include <cassert>
#include <cstdint>

#define RUDP_GET_MACRO(_1, _2, NAME, ...) NAME
#define RUDP_ASSERT(...) RUDP_GET_MACRO(__VA_ARGS__, RUDP_ASSERT2, RUDP_ASSERT1)(__VA_ARGS__)
#define RUDP_ASSERT1(cond) RUDP_ASSERT2(cond, "Assertion failed")
#define RUDP_ASSERT2(cond, msg) assert((cond) && __FILE__ ":" RUDP_STRINGIFY(__LINE__) ": " msg)

#define RUDP_STATIC_ASSERT(cond, msg) \
    static_assert(cond, __FILE__ ":" RUDP_STRINGIFY(__LINE__) ": " msg)
#define RUDP_UNREACHABLE() RUDP_ASSERT(false, "Unreachable");

#define RUDP_STRINGIFY(x) RUDP_STRINGIFY2(x)
#define RUDP_STRINGIFY2(x) #x

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

using linuxfd_t = s32;
using rudpfd_t = s32;

// TODO: Reconsider this organisation.
namespace internal {
    namespace constants {
        inline constexpr s32 UNINITIALISED_FD = -1;
        inline constexpr u32 PHI = 0x9e3779b9;
    }  // namespace constants

    inline bool is_valid_sockfd(linuxfd_t fd) {
        int opt;
        socklen_t opt_len = sizeof(opt);
        return getsockopt(fd, SOL_SOCKET, SO_TYPE, &opt, &opt_len) != -1;
    }
}  // namespace internal

RUDP_STATIC_ASSERT(internal::constants::UNINITIALISED_FD < 0, "Uninitialised FD must be negative.");

}  // namespace rudp
