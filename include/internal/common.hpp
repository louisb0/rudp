#pragma once

#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include <cassert>
#include <cerrno>
#include <chrono>
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

namespace internal::constants {
    inline constexpr s32 UNINITIALISED_FD = -1;

    inline constexpr u8 MAX_RETRANSMITS = 5;
    inline constexpr u16 MAX_DATA_BYTES = 1024;
    inline constexpr std::chrono::milliseconds RETRANSMIT_TIME = std::chrono::milliseconds(5000);

    inline constexpr sockaddr_in UNINITIALISED_PEER = {
        .sin_family = AF_UNSPEC,
        .sin_port = 0,
        .sin_addr = {0},
        .sin_zero{},
    };
}  // namespace internal::constants

}  // namespace rudp
