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
    assert_external_state(__PRETTY_FUNCTION__);

    while (true) {
        sockaddr_in peer_addr{};

        std::optional<packet> packet_opt = packet::recvfrom(m_fd, &peer_addr);
        if (!packet_opt.has_value()) {
            RUDP_ASSERT(
                errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR,
                "recvfrom() can only fail due to non-blocking or interruption, but we got %s.",
                strerror(errno));
            break;
        }

        if (peer_addr.sin_family != AF_INET) {
            continue;
        }

        packet packet = packet_opt.value();
        if (packet.header.flags != static_cast<u8>(flag::SYN)) {
            continue;
        }

        // Create and bind a new FD for the connection to be spawned.
        linuxfd_t fd = create_raw_socket();
        if (fd < 0) {
            continue;
        }

        struct sockaddr_in bound_addr{};
        bound_addr.sin_family = AF_INET;
        bound_addr.sin_addr.s_addr = INADDR_ANY;
        bound_addr.sin_port = 0;

        if (bind(fd, reinterpret_cast<struct sockaddr *>(&bound_addr), sizeof(bound_addr)) < 0) {
            close(fd);
            continue;
        }

        // Create and register the connection.
        auto connection = std::make_unique<internal::connection>(fd);
        if (!connection) {
            close(fd);
            continue;
        }

        auto [err, event_loop] = event_loop::instance();
        RUDP_ASSERT(err == event_loop::result::error::none && event_loop != nullptr,
                    "assert_external_state() guarantees an event loop.");

        if (!event_loop->add_handler(
                handler_type::connection, fd,
                [connection = connection.get()]() { connection->handle_events(); })) {
            close(fd);
            continue;
        }

        // Respond to the SYN, create the socket, and signal the user thread.
        if (!connection->passive_open(peer_addr)) {
            event_loop->remove_handler(handler_type::connection, fd);
            close(fd);
            continue;
        }

        rudpfd_t newfd = g_next_fd++;
        g_sockets[newfd] = internal::socket{std::move(connection)};

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
