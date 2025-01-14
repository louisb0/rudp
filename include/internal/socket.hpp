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
    } data;

    socket() : state(state::CREATED), data() {}
    ~socket() {
        if (state == state::LISTENING) {
            data.lstnr.std::unique_ptr<listener>::unique_ptr::~unique_ptr();
        }
    }

    socket(const socket &) = delete;
    socket &operator=(const socket &) = delete;

    socket(socket &&other) noexcept : state(other.state) {
        switch (state) {
        case state::CREATED:
            break;
        case state::BOUND:
            data.bound_fd = other.data.bound_fd;
            break;
        case state::LISTENING:
            new (&data.lstnr) std::unique_ptr<internal::listener>(std::move(other.data.lstnr));
            break;
        case state::CONNECTED:
            data.conn = other.data.conn;
            break;
        }
        other.state = state::CREATED;
        other.data.bound_fd = constants::UNINITIALISED_FD;
    }

    socket &operator=(socket &&other) noexcept {
        if (this != &other) {
            if (state == state::LISTENING) {
                data.lstnr.~unique_ptr<internal::listener>();
            }

            state = other.state;
            switch (state) {
            case state::CREATED:
                break;
            case state::BOUND:
                data.bound_fd = other.data.bound_fd;
                break;
            case state::LISTENING:
                new (&data.lstnr) std::unique_ptr<internal::listener>(std::move(other.data.lstnr));
                break;
            case state::CONNECTED:
                data.conn = other.data.conn;
                break;
            }
            other.state = state::CREATED;
            other.data.bound_fd = constants::UNINITIALISED_FD;
        }
        return *this;
    }
};

extern rudpfd_t next_fd;
extern std::unordered_map<rudpfd_t, socket> g_sockets;

}  // namespace rudp::internal
