#pragma once

#include "internal/common.hpp"

namespace rudp::internal::testing {

class simulator {
public:
    float drop{};
    float corruption{};
    float duplication{};
    u16 min_latency_ms{};
    u16 max_latency_ms{};

    void reset();

    static simulator &instance() {
        static simulator instance;
        return instance;
    }

    [[nodiscard]] static ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                                        const sockaddr *addr, socklen_t addrlen);

private:
    [[nodiscard]] bool should_drop() const noexcept;
    [[nodiscard]] bool should_corrupt() const noexcept;
    [[nodiscard]] bool should_duplicate() const noexcept;
    void simulate_latency() const noexcept;
};

}  // namespace rudp::internal::testing
