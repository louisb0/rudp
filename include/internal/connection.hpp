#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstddef>
#include <cstring>
#include <memory>
#include <random>
#include <unordered_map>

#include "internal/assert.hpp"
#include "internal/common.hpp"
#include "internal/event_handler.hpp"
#include "internal/packet.hpp"

namespace rudp::internal {
namespace {
    // NOTE: Predictable after 624 generated numbers but this is the best PRNG supported by C++.
    u32 random_u32() {
        static std::random_device rd;
        static std::seed_seq ss{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
        static std::mt19937 gen(ss);
        static std::uniform_int_distribution<uint32_t> dis(1, std::numeric_limits<uint32_t>::max());
        return dis(gen);
    }
}  // namespace

// NOTE: I thought this was necessary so that if we can detect identical source/destination
// connections and prevent them from being reopened while in TIME_WAIT. However, our listeners spawn
// a handler on a new port, making this unlikely.
struct connection_tuple {
    const in_addr_t src_ip;
    const in_port_t src_port;
    const in_port_t dst_port;
    const in_addr_t dst_ip;

    [[nodiscard]] sockaddr *dst() const {
        static sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = dst_port;
        addr.sin_addr.s_addr = dst_ip;
        return reinterpret_cast<sockaddr *>(&addr);
    }

    [[nodiscard]] sockaddr *src() const {
        static sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = src_port;
        addr.sin_addr.s_addr = src_ip;
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
          m_state(state::closed),
          m_prev_state(state::closed),
          m_seqnum(random_u32()) {}

    void handle_events() noexcept;

    [[nodiscard]] bool syn() noexcept;
    [[nodiscard]] bool synack(packet_header pkt) noexcept;

    void wait_for_established() noexcept;

    void assert_state(const char *caller) const noexcept;

private:
    const connection_tuple m_tuple;

    enum class state {
        closed,
        syn_sent,
        syn_rcvd,
        established,
    };
    enum state m_state;
    enum state m_prev_state;

    u32 m_seqnum;

    [[nodiscard]] bool send_packet(packet_header pkt) noexcept;
};

extern std::unordered_map<connection_tuple, std::unique_ptr<connection>, connection_tuple_hash>
    g_connections;

}  // namespace rudp::internal
