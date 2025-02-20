#pragma once

#include <memory>
#include <unordered_map>
#include <variant>

#include "internal/common.hpp"
#include "internal/connection.hpp"
#include "internal/listener.hpp"

namespace rudp::internal {

struct socket {
    // clang-format off
    std::variant<
        std::monostate,                     // created
        linuxfd_t,                          // bound
        std::unique_ptr<class listener>,    // listening
        connection_tuple                    // connected
     > data;
    // clang-format on

    bool created() const noexcept {
        return std::holds_alternative<std::monostate>(data);
    }

    bool bound() const noexcept {
        return std::holds_alternative<linuxfd_t>(data);
    }

    bool listening() const noexcept {
        return std::holds_alternative<std::unique_ptr<class listener>>(data);
    }

    bool connected() const noexcept {
        return std::holds_alternative<connection_tuple>(data);
    }

    linuxfd_t fd() const noexcept {
        RUDP_ASSERT(bound(), "A socket must be bound for an underlying file descriptor to exist.");
        return std::get<linuxfd_t>(data);
    }

    class listener *listener() const noexcept {
        RUDP_ASSERT(listening(), "A socket must be listening for a listener to exist.");
        return std::get<std::unique_ptr<class listener>>(data).get();
    }

    const connection_tuple &tuple() const noexcept {
        RUDP_ASSERT(connected(), "A socket must be connected for a connection tuple to exist.");
        return std::get<connection_tuple>(data);
    }
};

linuxfd_t create_raw_socket();

extern rudpfd_t g_next_fd;
extern std::unordered_map<rudpfd_t, socket> g_sockets;

}  // namespace rudp::internal
