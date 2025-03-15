#include "internal/testing/simulator.hpp"

#include <random>
#include <thread>
#include <vector>

#include "internal/assert.hpp"
#include "internal/common.hpp"

namespace rudp::internal::testing {
namespace {
    [[nodiscard]] float random_float() {
        static std::mt19937 gen(std::random_device{}());
        static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(gen);
    }

    void corrupt(std::vector<u8> &packet) {
        if (packet.empty())
            return;

        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> num_corruptions(
            1, std::min(3, static_cast<int>(packet.size())));

        for (int i = 0; i < num_corruptions(gen); ++i) {
            std::uniform_int_distribution<size_t> pos_dist(0, packet.size() - 1);
            std::uniform_int_distribution<u8> val_dist(0, 255);

            packet[pos_dist(gen)] = val_dist(gen);
        }
    }
}  // namespace

void simulator::reset() {
    drop = {};
    corruption = {};
    duplication = {};
    min_latency_ms = {};
    max_latency_ms = {};
}

ssize_t simulator::sendto(int sockfd, const void *buf, size_t len, int flags, const sockaddr *addr,
                          socklen_t addrlen) {
    auto &sim = simulator::instance();

    if (sim.should_drop()) {
        return static_cast<ssize_t>(len);
    }

    std::vector<u8> data(static_cast<const u8 *>(buf), static_cast<const u8 *>(buf) + len);
    if (sim.should_corrupt()) {
        corrupt(data);
    }

    sim.simulate_latency();

    ssize_t result = ::sendto(sockfd, data.data(), data.size(), flags, addr, addrlen);
    if (result > 0 && sim.should_duplicate()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5 + rand() % 20));
        ::sendto(sockfd, data.data(), data.size(), flags, addr, addrlen);
    }

    return result;
}

bool simulator::should_drop() const noexcept {
    return random_float() < drop;
}

bool simulator::should_corrupt() const noexcept {
    return random_float() < corruption;
}

bool simulator::should_duplicate() const noexcept {
    return random_float() < duplication;
}

void simulator::simulate_latency() const noexcept {
    RUDP_ASSERT(min_latency_ms >= 0);
    RUDP_ASSERT(max_latency_ms >= min_latency_ms);

    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<u16> dist(min_latency_ms, max_latency_ms);

    std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
}

}  // namespace rudp::internal::testing
