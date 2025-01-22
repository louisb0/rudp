#pragma once

#include "internal/common.hpp"

namespace rudp::internal {

// NOTE: RUDP in ASCII.
inline constexpr u32 magic = 0x52554450;

enum flag : u8 {
    FIN = 1 << 0,
    SYN = 1 << 1,
    ACK = 1 << 2,
};

struct packet_header {
    u32 magic = internal::magic;
    u32 seqnum;
    u32 acknum;
    u8 flags;
};

}  // namespace rudp::internal
