#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstddef>
#include <unordered_map>

#include "internal/common.hpp"

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

class connection {
public:
    connection(connection_tuple tuple) : m_fd(constants::UNINITIALISED_FD), m_tuple(tuple) {}
    [[nodiscard]] bool init() noexcept;

    [[nodiscard]] bool assert_initialised_state(const char *caller) const noexcept;

private:
    linuxfd_t m_fd;
    const connection_tuple m_tuple;
};

extern std::unordered_map<connection_tuple, connection, connection_tuple_hash> g_connections;

}  // namespace rudp::internal
