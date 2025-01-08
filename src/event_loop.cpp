#include "internal/event_loop.hpp"

#include <sys/epoll.h>
#include <sys/socket.h>

#include <mutex>
#include <thread>

#include "internal/common.hpp"

namespace rudp::internal {

linuxfd_t epollfd{-1};

void event_loop() noexcept {
    RUDP_ASSERT(epollfd >= 0, "epoll not initialised");
}

bool ensure_event_loop() noexcept {
    static bool epoll_initalised{false};
    static std::once_flag flag;

    std::call_once(flag, []() {
        epollfd = epoll_create1(0);
        if (epollfd < 0) {
            return;
        }

        std::thread(event_loop).detach();
        epoll_initalised = true;
    });

    return epoll_initalised;
}

}  // namespace rudp::internal
