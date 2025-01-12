#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstddef>
#include <unordered_map>

#include "internal/common.hpp"

namespace rudp::internal {

struct connection_pair {
    in_addr_t src_ip;
    in_port_t src_port;
    in_port_t dst_port;
    in_addr_t dst_ip;

    bool operator==(const connection_pair &other) const {
        return src_ip == other.src_ip && src_port == other.src_port && dst_ip == other.dst_ip &&
               dst_port == other.dst_port;
    }
};

RUDP_STATIC_ASSERT(sizeof(connection_pair) == 12, "connection_pair must be tightly packed.");

struct connection_pair_hash {
    RUDP_STATIC_ASSERT(sizeof(in_port_t) == 2, "in_port_t must be 16-bit.");
    RUDP_STATIC_ASSERT(sizeof(in_addr_t) == 4, "in_addr_t must be 32-bit.");
    RUDP_STATIC_ASSERT(sizeof(in_port_t) * 2 == sizeof(in_addr_t),
                       "in_port_t must pack into in_addr_t.");

    static constexpr u32 PHI = 0x9e3779b9;

    size_t operator()(const connection_pair &t) const {
        u64 seed = t.src_ip;

        u32 ports = (u32(t.src_port) << 16) | u32(t.dst_port);
        seed ^= ports + PHI + (seed << 6) + (seed >> 2);
        seed ^= t.dst_ip + PHI + (seed << 6) + (seed >> 2);

        return seed;
    }
};

class connection {};

extern std::unordered_map<connection_pair, connection> g_connections;

}  // namespace rudp::internal
