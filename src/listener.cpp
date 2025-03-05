#include "internal/listener.hpp"

#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <sys/socket.h>

#include <cerrno>
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
    // TODO: Asserting which thread this is running from would be nice, but requires an event_loop
    // pointer. This is on the hot-path so I'd like to not call event_loop::instance() every call.
    assert_external_state(__PRETTY_FUNCTION__);

    while (true) {
        // Receive pending data.
        packet_header pkt;
        sockaddr_in connecting_addr{};
        socklen_t connecting_addr_len = sizeof(connecting_addr);

        ssize_t bytes =
            recvfrom(m_fd, &pkt, sizeof(pkt), 0,
                     reinterpret_cast<struct sockaddr *>(&connecting_addr), &connecting_addr_len);
        if (bytes < 0) {
            RUDP_ASSERT(
                errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR,
                "recvfrom() can only fail due to non-blocking or interruption, but we got %s.",
                strerror(errno));
            break;
        }

        if (connecting_addr.sin_family != AF_INET) {
            continue;
        }

        if (pkt.flags != flag::SYN) {
            continue;
        }

        // Create and bind a new FD for the connection to be spawned.
        linuxfd_t fd = create_raw_socket();
        if (fd < 0) {
            continue;
        }

        struct sockaddr_in bound_addr{};
        memset(&bound_addr, 0, sizeof(bound_addr));
        bound_addr.sin_family = AF_INET;
        bound_addr.sin_addr.s_addr = INADDR_ANY;
        bound_addr.sin_port = 0;

        if (bind(fd, reinterpret_cast<struct sockaddr *>(&bound_addr), sizeof(bound_addr)) < 0) {
            close(fd);
            continue;
        }

        // Create and initialise the connection.
        connection_tuple tuple = {
            .src_ip = bound_addr.sin_addr.s_addr,
            .src_port = bound_addr.sin_port,
            .dst_port = connecting_addr.sin_port,
            .dst_ip = connecting_addr.sin_addr.s_addr,
        };

        if (g_connections.contains(tuple)) {
            close(fd);
            continue;
        }

        // TODO: Revisit after connection refactor.
        auto connection = std::make_unique<internal::connection>(fd, tuple);
        if (!connection) {
            close(fd);
            continue;
        }
        connection->set_peer(connecting_addr);

        // Register the handler and respond.
        auto [err, event_loop] = event_loop::instance();
        RUDP_ASSERT(err == event_loop::result::error::none && event_loop != nullptr,
                    "assert_external_state() guarantees an event loop.");

        if (!event_loop->add_handler(
                handler_type::connection, fd,
                [connection = connection.get()]() { connection->handle_events(); })) {
            close(fd);
            continue;
        }

        if (!connection->synack(pkt)) {
            event_loop->remove_handler(handler_type::connection, fd);
            close(fd);
            continue;
        }

        // Register the connection and signal the user thread.
        RUDP_ASSERT(!g_connections.contains(tuple),
                    "A listener must not attempt to insert an existing connection.");
        g_connections.emplace(tuple, std::move(connection));

        rudpfd_t newfd = g_next_fd++;
        g_sockets[newfd] = internal::socket{tuple};

        m_ready.push(newfd);
        m_cv.notify_one();
    }
}

rudpfd_t listener::wait_and_accept() noexcept {
    assert_external_state(__PRETTY_FUNCTION__);

    // Block until there is a socket on the queue.
    std::unique_lock<std::mutex> lock(m_mtx);
    m_cv.wait(lock, [this]() { return !m_ready.empty(); });

    rudpfd_t fd = m_ready.front();
    m_ready.pop();
    return fd;
}

void listener::assert_external_state(const char *caller) const noexcept {
    auto [err, event_loop] = event_loop::instance();
    RUDP_ASSERT(err == event_loop::result::error::none && event_loop != nullptr,
                "A listener must not exist unless an event loop was succesfully created.");

    event_loop->assert_initialised_state(caller);
    event_loop->assert_handler_exists(caller, handler_type::listener, m_fd);

    RUDP_ASSERT(is_valid_sockfd(m_fd), "A listener's underlying file descriptor must be valid.");
}

}  // namespace rudp::internal
