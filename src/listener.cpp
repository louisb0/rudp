#include "internal/listener.hpp"

#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <sys/socket.h>

#include <condition_variable>
#include <cstring>
#include <mutex>
#include <queue>

#include "internal/assert.hpp"
#include "internal/common.hpp"
#include "internal/connection.hpp"
#include "internal/event_loop.hpp"
#include "internal/packet.hpp"
#include "internal/socket.hpp"

namespace rudp::internal {

void listener::handle_events() noexcept {
    assert_initialised_handler(__PRETTY_FUNCTION__);
    m_event_loop->assert_event_thread(__PRETTY_FUNCTION__);

    while (true) {
        // Receive pending data.
        packet_header pkt;
        sockaddr_in connecting_addr{};
        socklen_t connecting_addr_len = sizeof(connecting_addr);

        ssize_t bytes =
            recvfrom(m_fd, &pkt, sizeof(pkt), 0,
                     reinterpret_cast<struct sockaddr *>(&connecting_addr), &connecting_addr_len);
        if (bytes < 0 && errno == EWOULDBLOCK) {
            // TODO: What about other errno?
            break;
        }
        if (bytes == 0) {
            break;
        }

        if (connecting_addr.sin_family != AF_INET) {
            continue;
        }

        if (pkt.flags != flag::SYN) {
            continue;
        }

        // Create and bind a new FD for the connection to be spawned.
        // TODO: Consider RAII, e.g. internal::closing<linuxfd_t>
        linuxfd_t fd = internal::create_raw_socket();

        struct sockaddr_in new_addr;
        // TODO: Don't use memset.
        memset(&new_addr, 0, sizeof(new_addr));
        new_addr.sin_family = AF_INET;
        new_addr.sin_addr.s_addr = INADDR_ANY;
        new_addr.sin_port = 0;

        if (bind(fd, reinterpret_cast<struct sockaddr *>(&new_addr), sizeof(new_addr)) < 0) {
            close(fd);
            continue;
        }

        // Create and initialise the connection.
        internal::connection_tuple tuple = {
            .src_ip = new_addr.sin_addr.s_addr,
            .src_port = new_addr.sin_port,
            .dst_port = connecting_addr.sin_port,
            .dst_ip = connecting_addr.sin_addr.s_addr,
        };

        if (internal::g_connections.contains(tuple)) {
            close(fd);
            continue;
        }

        auto conn = std::make_unique<internal::connection>(fd, tuple);
        if (!conn) {
            close(fd);
            continue;
        }
        conn->set_peer(connecting_addr);

        // Register the handler and respond.
        if (!internal::event_loop::add_handler(conn.get())) {
            close(fd);
            continue;
        }
        conn->assert_initialised_handler(__PRETTY_FUNCTION__);

        if (!conn->synack(pkt)) {
            internal::event_loop::remove_handler(conn.get());
            close(fd);
            continue;
        }
        conn->assert_state(__PRETTY_FUNCTION__);

        auto [_, inserted] = internal::g_connections.emplace(tuple, std::move(conn));
        RUDP_ASSERT(inserted, "The listener will not insert an existing connection.");

        // Spawn the socket and signal the user thread.
        rudpfd_t newfd = internal::g_next_fd++;
        internal::socket sock;
        sock.data = tuple;

        internal::g_sockets[newfd] = std::move(sock);

        m_ready.push(newfd);
        m_cv.notify_one();
    }
}

rudpfd_t listener::wait_and_accept() noexcept {
    assert_initialised_handler(__PRETTY_FUNCTION__);
    m_event_loop->assert_user_thread(__PRETTY_FUNCTION__);

    // Block until there is a ready socket.
    std::unique_lock<std::mutex> lock(m_mtx);
    m_cv.wait(lock, [this]() { return !m_ready.empty(); });

    rudpfd_t fd = m_ready.front();
    m_ready.pop();
    return fd;
}

}  // namespace rudp::internal
