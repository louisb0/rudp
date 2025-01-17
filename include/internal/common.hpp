#pragma once

#include <sys/epoll.h>
#include <sys/socket.h>

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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
}  // namespace internal

}  // namespace rudp
