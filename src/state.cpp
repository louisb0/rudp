#include "internal/state.hpp"

#include <type_traits>
#include <unordered_map>

#include "internal/assert.hpp"
#include "internal/packet.hpp"

namespace rudp::internal {
namespace {
    struct pair_hash {
        template <class T1, class T2>
        std::size_t operator()(const std::pair<T1, T2> &p) const {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);
            return h1 ^ (h2 << 1);
        }
    };

};  // namespace

// clang-format off
const std::unordered_map<std::pair<state::kind, state::kind>, u8, pair_hash> g_transition_map = {
    {{state::kind::created,     state::kind::syn_sent},     static_cast<u8>(flag::SYN)},
    {{state::kind::created,     state::kind::syn_rcvd},     static_cast<u8>(flag::SYN) | static_cast<u8>(flag::ACK)},
    {{state::kind::syn_sent,    state::kind::established},  static_cast<u8>(flag::ACK)},

    {{state::kind::created,     state::kind::created},      state::NO_FLAGS},
    {{state::kind::syn_rcvd,    state::kind::established},  state::NO_FLAGS},
};
// clang-format on

void state::transition(state::kind to) noexcept {
    std::pair<kind, kind> transition = {current(), to};

    RUDP_ASSERT(g_transition_map.contains(transition), "Invalid state transition from %d to %d.",
                static_cast<std::underlying_type_t<kind>>(transition.first),
                static_cast<std::underlying_type_t<kind>>(transition.second));

    m_transitions.push_back(transition);
}

[[nodiscard]] u8 state::derive_flags() noexcept {
    u8 flags{};
    for (size_t i = m_last_read_index; i < m_transitions.size(); i++) {
        flags |= g_transition_map.at(m_transitions[i]);
    }

    m_last_read_index = m_transitions.size();
    return flags;
}

[[nodiscard]] state::kind state::current() const noexcept {
    return m_transitions.back().second;
}

}  // namespace rudp::internal
