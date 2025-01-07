#pragma once

#include <chrono>
#include <optional>

#include "packet.hpp"

namespace rudp::internal {

constexpr std::chrono::milliseconds retransmit_timeout{1000};
constexpr u8 max_retransmits{5};

struct transmission_state {
    socket* sock;
    packet pkt;
    flag_t expected_flag;

    u8 retransmits;
    std::chrono::steady_clock::time_point sent_at;
};

void reliably_send(socket* const sock, packet&& pkt) noexcept;
[[nodiscard]] std::optional<packet> reliably_recv(socket* const sock) noexcept;

void retransmit() noexcept;

}  // namespace rudp::internal
