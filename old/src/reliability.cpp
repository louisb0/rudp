#include "reliability.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>

#include <chrono>
#include <cstring>
#include <optional>
#include <queue>
#include <unordered_map>

#include "common.hpp"
#include "packet.hpp"

namespace rudp::internal {

struct connection_state {
    std::unordered_map<u32, transmission_state> sent_packets;
    std::unordered_map<u32, packet> received_packets;
    u32 next_expected_seq;
    socket* sock;
};

static std::unordered_map<linuxfd_t, connection_state> s_connections;

static connection_state& connection(socket* const sock) {
    auto it = s_connections.find(sock->fd);
    if (it == s_connections.end()) {
        connection_state state;
        state.next_expected_seq = 0;
        state.sock = sock;

        auto [inserted_it, _] = s_connections.insert({sock->fd, std::move(state)});
        return inserted_it->second;
    }

    return it->second;
}

static flag_t get_expected_flag(flag_t sent) {
    switch (sent) {
    case SYN:
        return SYN | ACK;
    case SYN | ACK:
        return ACK;
    default:
        UNREACHABLE();
    }
}

void reliably_send(socket* const sock, packet&& pkt) noexcept {
    auto& conn = connection(sock);

    // Don't retransmit ACKs
    if (pkt.flags != ACK) {
        transmission_state& tx_state = conn.sent_packets[sock->seq_num];
        tx_state.sock = sock;
        tx_state.pkt = std::move(pkt);
        tx_state.expected_flag = get_expected_flag(pkt.flags);
        tx_state.sent_at = std::chrono::steady_clock::now();
        tx_state.retransmits = 0;
    }

    sendto(sock->fd, &pkt, sizeof(pkt), 0, reinterpret_cast<sockaddr*>(&sock->peer.addr),
           sock->peer.addr_len);

    sock->seq_num++;
}

std::optional<packet> reliably_recv(socket* const sock) noexcept {
    auto& conn = connection(sock);
    packet pkt;

    // Query for and return packets with sequence numbers ahead of what we expected.
    if (auto it = conn.received_packets.find(conn.next_expected_seq);
        it != conn.received_packets.end()) {
        packet stored_pkt = it->second;

        conn.received_packets.erase(it);
        conn.next_expected_seq++;

        return stored_pkt;
    }

    // If no stored packets, try to receive a new one.
    if (recvfrom(sock->fd, &pkt, sizeof(pkt), 0, reinterpret_cast<sockaddr*>(&sock->peer.addr),
                 &sock->peer.addr_len) <= 0) {
        return std::nullopt;
    }

    // Store all non-ACK packets, e.g. for future ahead of sequence queries.
    if (!(pkt.flags == ACK)) {
        conn.received_packets[pkt.seq_num] = pkt;
    }

    // The initial packet (SYN) has no history to validate against.
    if (pkt.flags == SYN) {
        conn.next_expected_seq = pkt.seq_num + 1;
        return pkt;
    }

    // Handle ACKs by removing acknowledged packets.
    if (pkt.flags & ACK) {
        auto it = conn.sent_packets.find(pkt.ack_num - 1);
        if (it != conn.sent_packets.end() && it->second.expected_flag == pkt.flags) {
            conn.sent_packets.erase(it);
        }
        return pkt;
    }

    // Retransmitted packet
    if (pkt.seq_num < conn.next_expected_seq) {
        return pkt;
    }

    // This is the packet we were expecting
    if (pkt.seq_num == conn.next_expected_seq) {
        conn.next_expected_seq++;
        return pkt;
    }

    // Packet from future - we stored it above but won't process it yet
    return std::nullopt;
}

void retransmit() noexcept {
    auto now = std::chrono::steady_clock::now();

    for (auto& [_, conn] : s_connections) {
        std::queue<u32> resends;

        for (const auto& [seq_num, tx_state] : conn.sent_packets) {
            if (tx_state.retransmits >= max_retransmits) {
                // TODO: Shutdown the connection, signal failure
                continue;
            }

            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - tx_state.sent_at);

            if (elapsed >= retransmit_timeout) {
                resends.push(seq_num);
            }
        }

        while (!resends.empty()) {
            u32 seq_num = resends.front();
            resends.pop();

            transmission_state& tx_state = conn.sent_packets[seq_num];
            tx_state.retransmits++;

            s64 n = sendto(tx_state.sock->fd, &tx_state.pkt, sizeof(tx_state.pkt), 0,
                           reinterpret_cast<sockaddr*>(&tx_state.sock->peer.addr),
                           tx_state.sock->peer.addr_len);

            if (n > 0) {
                tx_state.sent_at = now;
            }
        }
    }
}

}  // namespace rudp::internal
