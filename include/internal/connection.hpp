#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstddef>
#include <unordered_map>

#include "internal/common.hpp"

namespace rudp::internal {

struct connection_tuple {
    in_addr_t src_ip;
    in_port_t src_port;
    in_port_t dst_port;
    in_addr_t dst_ip;

    bool operator==(const connection_tuple &other) const {
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

    static constexpr u32 PHI = 0x9e3779b9;

    size_t operator()(const connection_tuple &t) const {
        u64 seed = t.src_ip;

        u32 ports = (u32(t.src_port) << 16) | u32(t.dst_port);
        seed ^= ports + PHI + (seed << 6) + (seed >> 2);
        seed ^= t.dst_ip + PHI + (seed << 6) + (seed >> 2);

        return seed;
    }
};

class connection {
public:
    connection(connection_tuple tuple) : m_fd(constants::UNINITIALISED_FD), m_tuple(tuple) {}
    [[nodiscard]] bool init() noexcept;

    [[nodiscard]] bool assert_state();

private:
    linuxfd_t m_fd;
    connection_tuple m_tuple;
};

extern std::unordered_map<connection_tuple, connection> g_connections;

}  // namespace rudp::internal
