#pragma once

#include <memory>
#include <unordered_map>

#include "internal/common.hpp"
#include "internal/connection.hpp"
#include "internal/listener.hpp"

namespace rudp::internal {

// NOTE: std::variant is a valid alternative but you supposedly pay for it's convenience with speed
// (though we would need to measure it to be sure).
struct socket {
    enum class state { created, bound, listening, connected } state;
    union storage {
        linuxfd_t bound_fd;
        std::unique_ptr<listener> lstnr;
        connection_tuple conn;

        storage() noexcept : bound_fd(constants::UNINITIALISED_FD) {}
        ~storage() noexcept {}

        void destroy(enum state s) const noexcept {
            if (s == state::listening) {
                lstnr.~unique_ptr<listener>();
            }
        }

        void construct_from(enum state s, storage &&other) noexcept {
            switch (s) {
            case state::created:
                bound_fd = constants::UNINITIALISED_FD;
                break;
            case state::bound:
                bound_fd = other.bound_fd;
                break;
            case state::listening:
                new (&lstnr) std::unique_ptr<listener>(std::move(other.lstnr));
                break;
            case state::connected:
                new (&conn) connection_tuple(other.conn);
                break;
            }
        }
    } data;

    socket() noexcept : state(state::created), data() {}
    ~socket() noexcept {
        data.destroy(state);
    }

    socket(const socket &) = delete;
    socket &operator=(const socket &) = delete;

    socket(socket &&other) noexcept : state(other.state) {
        data.construct_from(state, std::move(other.data));
        other.state = state::created;
    }

    socket &operator=(socket &&other) noexcept {
        if (this != &other) {
            data.destroy(state);
            state = other.state;
            data.construct_from(state, std::move(other.data));
            other.state = state::created;
        }
        return *this;
    }
};

linuxfd_t create_raw_socket();

extern rudpfd_t next_fd;
extern std::unordered_map<rudpfd_t, socket> g_sockets;

}  // namespace rudp::internal
