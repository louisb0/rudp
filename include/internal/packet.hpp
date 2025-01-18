#pragma once

#include "internal/common.hpp"

namespace rudp::internal {

enum flag : u8 {
    FIN = 1 << 0,
    SYN = 1 << 1,
    ACK = 1 << 2,
};

struct packet_header {
    u32 seqnum;
    u32 acknum;
    u8 flags;
};

}  // namespace rudp::internal
