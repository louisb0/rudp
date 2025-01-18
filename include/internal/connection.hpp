#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstddef>
#include <memory>
#include <unordered_map>

#include "internal/assert.hpp"
#include "internal/common.hpp"
#include "internal/event_handler.hpp"
#include "internal/packet.hpp"

namespace rudp::internal {

struct connection_tuple {
    const in_addr_t src_ip;
    const in_port_t src_port;
    const in_port_t dst_port;
    const in_addr_t dst_ip;

    [[nodiscard]] bool operator==(const connection_tuple &other) const noexcept {
        return src_ip == other.src_ip && src_port == other.src_port && dst_ip == other.dst_ip &&
               dst_port == other.dst_port;
    }
};

RUDP_STATIC_ASSERT(sizeof(connection_tuple) == 12, "connection_tuple must be tightly packed.");

struct connection_tuple_hash {
    RUDP_STATIC_ASSERT(sizeof(in_port_t) == 2, "in_port_t must be 16-bit.");
    RUDP_STATIC_ASSERT(sizeof(in_addr_t) == 4, "in_addr_t must be 32-bit.");
    RUDP_STATIC_ASSERT(sizeof(in_port_t) * 2 == sizeof(in_addr_t),
                       "in_port_t must pack into in_addr_t.");
    RUDP_STATIC_ASSERT(sizeof(size_t) >= sizeof(u64),
                       "size_t must be large enough to hold 64-bit hash value.");

    [[nodiscard]] size_t operator()(const connection_tuple &t) const noexcept {
        u64 seed = t.src_ip;

        u32 ports = (u32(t.src_port) << 16) | u32(t.dst_port);
        seed ^= ports + constants::PHI + (seed << 6) + (seed >> 2);
        seed ^= t.dst_ip + constants::PHI + (seed << 6) + (seed >> 2);

        return seed;
    }
};

class connection : public event_handler {
public:
    connection(linuxfd_t fd, connection_tuple tuple) noexcept
        : event_handler(fd),
          m_tuple(tuple),
          m_state(state::closed),
          m_prev_state(state::closed),
          m_seqnum(0) {}  // TODO: Randomise. We will need a RNG for traces anyway.

    void handle_event() noexcept;

    [[nodiscard]] bool on_syn(packet_header pkt) noexcept;

    void assert_state(const char *caller) const noexcept;

private:
    linuxfd_t m_fd;
    const connection_tuple m_tuple;

    enum class state {
        closed,
        syn_rcvd,
    };
    enum state m_state;
    enum state m_prev_state;

    u32 m_seqnum;

    // TODO: This will need to change to packet, eventually.
    [[nodiscard]] bool send_packet(packet_header pkt) noexcept;
};

extern std::unordered_map<connection_tuple, std::unique_ptr<connection>, connection_tuple_hash>
    g_connections;

}  // namespace rudp::internal
