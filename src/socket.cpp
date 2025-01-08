#include "internal/socket.hpp"

#include <sys/epoll.h>
#include <sys/socket.h>

#include "internal/common.hpp"
#include "internal/event_loop.hpp"

namespace rudp::internal {

socket_manager &socket_manager::instance() noexcept {
    static socket_manager manager;
    return manager;
}

rudpfd_t socket_manager::create() noexcept {
    linuxfd_t fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    rudpfd_t new_fd = m_next_fd++;

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.u32 = new_fd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        close(fd);
        return -1;
    }

    internal::socket socket{};
    socket.fd = fd;
    socket.state = socket::CLOSED;

    m_sockets.emplace(new_fd, std::move(socket));

    return new_fd;
}

internal::socket *socket_manager::get(rudpfd_t fd) noexcept {
    auto it = m_sockets.find(fd);
    if (it == m_sockets.end()) {
        return nullptr;
    }
    return &it->second;
}

}  // namespace rudp::internal
