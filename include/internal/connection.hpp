#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "internal/common.hpp"
#include "internal/packet.hpp"
#include "internal/state.hpp"

namespace rudp::internal {

struct sent_packet {
    class packet packet;
    std::chrono::steady_clock::time_point sent_at;
    u8 retransmits;
};

struct received_packet {
    class packet packet;
    sockaddr_in peer;
};

class connection {
public:
    connection(linuxfd_t fd) : m_fd(fd) {}

    void handle_events() noexcept;
    void buffer_pending() noexcept;
    void retransmit() noexcept;

    [[nodiscard]] bool passive_open(const sockaddr_in &peer) noexcept;
    [[nodiscard]] bool active_open(const sockaddr_in &listening_peer) noexcept;
    void wait_for_established() noexcept;

    [[nodiscard]] const sockaddr_in &peer() const noexcept;

private:
    const linuxfd_t m_fd;

    state m_state{};
    u32 m_seqnum{};
    u32 m_acknum{};

    // NOTE: connection::listener will spawn new connections on an ephemeral kernel port, meaning
    // that we do not know the port of our peer until we first receive a valid packet from them.
    sockaddr_in m_peer{constants::UNINITIALISED_PEER};

    std::unordered_map<u32, sent_packet> m_sent;
    std::map<u32, received_packet> m_received;
    std::deque<u8> m_buffer;

    std::mutex m_mtx;
    std::condition_variable m_cv;

    void handle_ack(const packet &packet) noexcept;
    void handle_synack(const packet &packet, const sockaddr_in &peer) noexcept;

    bool send_store_packet(u8 flags, std::optional<sockaddr_in> to = std::nullopt) noexcept;

    void assert_external_state(const char *caller) const noexcept;
};

extern std::map<std::chrono::steady_clock::time_point, std::unique_ptr<class connection>>
    g_time_wait_connections;

}  // namespace rudp::internal
