#pragma once

#include <utility>
#include <vector>

#include "internal/common.hpp"

namespace rudp::internal {

class state {
public:
    static constexpr u8 NO_FLAGS = 0x00;
    enum class kind { created, syn_sent, syn_rcvd, established };

    state() : m_last_read_index(0) {
        m_transitions.push_back({kind::created, kind::created});
    }

    void transition(kind to) noexcept;
    [[nodiscard]] u8 derive_flags() noexcept;
    [[nodiscard]] kind current() const noexcept;

private:
    std::vector<std::pair<kind, kind>> m_transitions;
    size_t m_last_read_index;
};

}  // namespace rudp::internal
