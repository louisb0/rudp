#include "internal/state.hpp"

#include <type_traits>

#include "internal/assert.hpp"
#include "internal/packet.hpp"

// NOTE: A map here is a lot nicer; we can map from a pair of states to the flags or NO_FLAGS.
// However, I kept getting what I assume are SIOF issues which I can't quite figure out.
namespace rudp::internal {
namespace {
    bool is_valid_transition(state::kind from, state::kind to) {
        // clang-format off
        static const bool transition_matrix[4][4] = {
            // to: created,  syn_sent, syn_rcvd, established
            {   true,      true,     true,     false   },  // from: created
            {   false,     false,    false,    true    },  // from: syn_sent
            {   false,     false,    false,    true    },  // from: syn_rcvd
            {   false,     false,    false,    false   }   // from: established
        };
        // clang-format on

        int from_i = static_cast<int>(from);
        int to_i = static_cast<int>(to);

        if (from_i < 0 || from_i > 3 || to_i < 0 || to_i > 3) {
            return false;
        }

        return transition_matrix[from_i][to_i];
    }

    u8 get_transition_flags(state::kind from, state::kind to) {
        if (from == state::kind::created && to == state::kind::syn_sent) {
            return static_cast<u8>(flag::SYN);
        }
        if (from == state::kind::created && to == state::kind::syn_rcvd) {
            return static_cast<u8>(flag::SYN) | static_cast<u8>(flag::ACK);
        }
        if (from == state::kind::syn_sent && to == state::kind::established) {
            return static_cast<u8>(flag::ACK);
        }

        return state::NO_FLAGS;
    }
}  // namespace

void state::transition(state::kind to) noexcept {
    state::kind from = current();

    RUDP_ASSERT(is_valid_transition(from, to), "Invalid state transition from %d to %d.",
                static_cast<std::underlying_type_t<kind>>(from),
                static_cast<std::underlying_type_t<kind>>(to));

    m_transitions.push_back({from, to});
}

[[nodiscard]] u8 state::derive_flags() noexcept {
    u8 flags{};
    for (size_t i = m_last_read_index; i < m_transitions.size(); i++) {
        auto &transition = m_transitions[i];
        flags |= get_transition_flags(transition.first, transition.second);
    }

    m_last_read_index = m_transitions.size();
    return flags;
}

[[nodiscard]] state::kind state::current() const noexcept {
    return m_transitions.back().second;
}

}  // namespace rudp::internal
