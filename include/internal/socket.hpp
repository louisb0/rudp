#pragma once

#include <memory>
#include <unordered_map>

#include "internal/common.hpp"
#include "internal/connection.hpp"
#include "internal/listener.hpp"

namespace rudp::internal {

// NOTE: std::variant is a valid alternative. I've heard it is slow but it would require measuring
// in our use-case. I'm comfortable with this for now.
struct socket {
    enum class state { CREATED, BOUND, LISTENING, CONNECTED } state;
    union storage {
        linuxfd_t bound_fd;
        std::unique_ptr<listener> lstnr;
        connection_tuple conn;

        storage() : bound_fd(constants::UNINITIALISED_FD) {}
        ~storage() {}

        void destroy(enum state s) const noexcept {
            if (s == state::LISTENING) {
                lstnr.~unique_ptr<listener>();
            }
        }

        void construct_from(enum state s, storage &&other) noexcept {
            switch (s) {
            case state::CREATED:
                bound_fd = constants::UNINITIALISED_FD;
                break;
            case state::BOUND:
                bound_fd = other.bound_fd;
                break;
            case state::LISTENING:
                new (&lstnr) std::unique_ptr<listener>(std::move(other.lstnr));
                break;
            case state::CONNECTED:
                new (&conn) connection_tuple(other.conn);
                break;
            }
        }
    } data;

    socket() : state(state::CREATED), data() {}
    ~socket() {
        data.destroy(state);
    }

    socket(const socket &) = delete;
    socket &operator=(const socket &) = delete;

    socket(socket &&other) noexcept : state(other.state) {
        data.construct_from(state, std::move(other.data));
        other.state = state::CREATED;
    }

    socket &operator=(socket &&other) noexcept {
        if (this != &other) {
            data.destroy(state);
            state = other.state;
            data.construct_from(state, std::move(other.data));
            other.state = state::CREATED;
        }
        return *this;
    }
};

extern rudpfd_t next_fd;
extern std::unordered_map<rudpfd_t, socket> g_sockets;

}  // namespace rudp::internal
