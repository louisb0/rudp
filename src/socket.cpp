#include "internal/socket.hpp"

#include <sys/epoll.h>
#include <sys/socket.h>

#include <condition_variable>
#include <memory>
#include <queue>
#include <unordered_map>

#include "internal/common.hpp"
#include "internal/event_loop.hpp"

namespace rudp::internal {

socket_manager &socket_manager::instance() noexcept {
    static socket_manager manager;
    return manager;
}

rudpfd_t socket_manager::create() noexcept {
    bool epoll_initalised = ensure_event_loop();
    if (!epoll_initalised) {
        RUDP_ASSERT(epollfd < 0, "errno should be propogated");
        return -1;
    }
    RUDP_ASSERT(epollfd >= 0, "epoll not initialised");

    linuxfd_t fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    rudpfd_t new_fd = m_next_fd++;

    RUDP_ASSERT(m_sockets.find(new_fd) == m_sockets.end(),
                "attempting to create with duplicate rudpfd");

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = new_fd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        close(fd);
        return -1;
    }

    internal::socket socket{
        .fd = fd,
        .state = socket::CLOSED,
        .listen_data =
            {
                .backlog = -1,
                .queue = std::queue<rudpfd_t>{},
            },
        .mtx = std::make_unique<std::mutex>(),
        .cv = std::make_unique<std::condition_variable>(),
    };

    RUDP_ASSERT(m_sockets.find(new_fd) == m_sockets.end(), "fd already exists");
    m_sockets.emplace(new_fd, std::move(socket));

    return new_fd;
}

internal::socket *socket_manager::get(rudpfd_t fd) noexcept {
    auto it = m_sockets.find(fd);
    if (it == m_sockets.end()) {
        return nullptr;
    }

    socket *socket = &it->second;
    RUDP_ASSERT(socket->fd >= 0, "all sockets should have a valid underlying fd");

    return socket;
}

bool socket_manager::destroy(rudpfd_t fd) noexcept {
    auto it = m_sockets.find(fd);
    RUDP_ASSERT(it != m_sockets.end(), "attempting to destroy a non-existent socket");

    socket &socket = it->second;
    RUDP_ASSERT(socket.fd >= 0, "all sockets should have a valid underlying fd");

    // TODO: Sockets in TIME_WAIT should be held for 2MSL and prevent us from reopening a connection
    // with that peer. We are currently closing it early.
    RUDP_ASSERT(socket.state == socket::CLOSED || socket.state == socket::TIME_WAIT,
                "destroying an open socket");

    int result = ::close(socket.fd);
    if (result < 0) {
        return false;
    }

    m_sockets.erase(it);
    return true;
}

}  // namespace rudp::internal
