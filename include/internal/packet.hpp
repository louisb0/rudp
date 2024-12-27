#pragma once

#include "common.hpp"
#include "socket.hpp"

namespace rudp::internal {

using flag_t = u8;

constexpr flag_t NONE = 0x00;
constexpr flag_t SYN = 0x01;
constexpr flag_t ACK = 0x02;

struct packet {
    u32 seq_num;
    u32 ack_num;
    u8 flags;
    u8 data[128];
} __attribute__((packed));

[[nodiscard]] packet create_syn(const socket* const sock) noexcept;
[[nodiscard]] packet create_synack(const socket* const sock) noexcept;
[[nodiscard]] packet create_ack(const socket* const sock) noexcept;

}  // namespace rudp::internal
