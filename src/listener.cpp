#include "internal/listener.hpp"

#include <sys/epoll.h>
#include <sys/socket.h>

#include <condition_variable>
#include <mutex>
#include <queue>

#include "internal/common.hpp"
#include "internal/connection.hpp"
#include "internal/event_loop.hpp"
#include "internal/packet.hpp"

namespace rudp::internal {

void listener::handle_events() noexcept {
    assert_initialised_handler(__PRETTY_FUNCTION__);
    m_event_loop->assert_event_thread(__PRETTY_FUNCTION__);

    while (true) {
        // Receive pending data.
        packet_header pkt;
        sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);

        ssize_t bytes = recvfrom(m_fd, &pkt, sizeof(pkt), 0,
                                 reinterpret_cast<struct sockaddr *>(&addr), &addr_len);
        if (bytes < 0 && errno == EWOULDBLOCK) {
            continue;
        }

        if (bytes == 0) {
            continue;
        }

        if (addr.ss_family != AF_INET) {
            continue;
        }

        if (pkt.flags != flag::SYN) {
            continue;
        }

        // Create a new FD, bound to the same address, to be used for the established connection.
        sockaddr_storage local_addr;
        socklen_t local_addr_len = sizeof(local_addr);
        if (getsockname(m_fd, reinterpret_cast<struct sockaddr *>(&local_addr), &local_addr_len) <
            0) {
            continue;
        }
        RUDP_ASSERT(local_addr.ss_family == AF_INET, "All listeners should be created using IPv4.");

        int fd = socket(local_addr.ss_family, SOCK_DGRAM, 0);
        if (fd < 0) {
            continue;
        }

        if (bind(fd, reinterpret_cast<struct sockaddr *>(&local_addr), local_addr_len) < 0) {
            close(fd);
            continue;
        }

        // Create and initialise the connection.
        const struct sockaddr_in *local_in =
            reinterpret_cast<const struct sockaddr_in *>(&local_addr);
        const struct sockaddr_in *peer_in = reinterpret_cast<const struct sockaddr_in *>(&addr);

        internal::connection_tuple tuple = {
            .src_ip = local_in->sin_addr.s_addr,
            .src_port = local_in->sin_port,
            .dst_port = peer_in->sin_port,
            .dst_ip = peer_in->sin_addr.s_addr,
        };

        // NOTE: This logic is identical to that of rudp.cpp in connect() with the added
        // requirement of closing the FD. I don't think it requires abstraction yet as the
        // overhead of conditional RAII doesn't outweigh a singular duplicate.
        auto conn = std::make_unique<internal::connection>(fd, tuple);
        if (!conn || !internal::event_loop::add_handler(conn.get())) {
            close(fd);
            continue;
        }
        conn->assert_initialised_handler(__PRETTY_FUNCTION__);

        auto [_, inserted] = internal::g_connections.emplace(tuple, std::move(conn));
        if (!inserted) {
            close(fd);
            internal::event_loop::remove_handler(conn.get());
            continue;
        }

        // Handle the received SYN.
        if (!conn->on_syn(pkt)) {
            close(fd);
            continue;
        }
        conn->assert_state(__PRETTY_FUNCTION__);
    }
}

rudpfd_t listener::wait_and_accept() noexcept {
    assert_initialised_handler(__PRETTY_FUNCTION__);
    m_event_loop->assert_user_thread(__PRETTY_FUNCTION__);

    std::unique_lock<std::mutex> lock(m_mtx);
    m_cv.wait(lock, [this]() { return !m_ready.empty(); });

    rudpfd_t fd = m_ready.front();
    m_ready.pop();
    return fd;
}

}  // namespace rudp::internal
