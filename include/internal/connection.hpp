#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstddef>
#include <cstring>
#include <memory>
#include <unordered_map>

#include "internal/assert.hpp"
#include "internal/common.hpp"
#include "internal/event_handler.hpp"
#include "internal/packet.hpp"

namespace rudp::internal {

// NOTE: I thought this was necessary so that if we can detect identical source/destination
// connections and prevent them from being reopened while in TIME_WAIT. However, our listeners spawn
// a handler on a new port, making this unlikely.
// TODO: This smells.
struct connection_tuple {
    in_addr_t src_ip;
    in_port_t src_port;
    in_port_t dst_port;
    in_addr_t dst_ip;

    // TODO: Replace with is_peer() or similar.
    [[nodiscard]] bool equals_dst(sockaddr_in *addr) const {
        return addr->sin_port == dst_port && addr->sin_addr.s_addr == dst_ip;
    }

    [[nodiscard]] sockaddr *dst() const {
        static sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = dst_port;
        addr.sin_addr.s_addr = dst_ip;
        return reinterpret_cast<sockaddr *>(&addr);
    }

    static constexpr socklen_t addr_size() {
        return sizeof(sockaddr_in);
    }

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
          m_peer_known(false),
          m_state(state::closed),
          m_prev_state(state::closed),
          m_seqnum(0) {}

    // TODO: Remove.
    void handle_events() noexcept;

    [[nodiscard]] bool syn() noexcept;
    [[nodiscard]] bool synack(packet_header pkt) noexcept;

    void wait_for_established() noexcept;
    void set_peer(const sockaddr_in &addr);

    void assert_state(const char *caller) const noexcept;

private:
    connection_tuple m_tuple;
    bool m_peer_known;

    enum class state {
        closed,
        syn_sent,
        syn_rcvd,
        established,
    };
    // TODO: This is a weird coupling which could be abstracted into state itself.
    enum state m_state;
    enum state m_prev_state;

    u32 m_seqnum;

    [[nodiscard]] bool send_packet(packet_header pkt) noexcept;
};

extern std::unordered_map<connection_tuple, std::unique_ptr<connection>, connection_tuple_hash>
    g_connections;

}  // namespace rudp::internal
