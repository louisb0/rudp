#include "internal/listener.hpp"

#include <sys/epoll.h>
#include <sys/socket.h>

#include <condition_variable>
#include <mutex>
#include <queue>

#include "internal/common.hpp"
#include "internal/event_loop.hpp"

namespace rudp::internal {

void listener::handle_event() noexcept {
    RUDP_ASSERT(assert_initialised_handler(__PRETTY_FUNCTION__));
    RUDP_ASSERT(m_event_loop->assert_correct_thread(__PRETTY_FUNCTION__));
}

rudpfd_t listener::wait_and_accept() noexcept {
    RUDP_ASSERT(assert_initialised_handler(__PRETTY_FUNCTION__));

    std::unique_lock<std::mutex> lock(m_mtx);
    m_cv.wait(lock, [this]() { return !m_ready.empty(); });

    rudpfd_t fd = m_ready.front();
    m_ready.pop();
    return fd;
}

}  // namespace rudp::internal
