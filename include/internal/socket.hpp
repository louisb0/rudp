#pragma once

#include <memory>
#include <unordered_map>
#include <variant>

#include "internal/common.hpp"
#include "internal/connection.hpp"
#include "internal/listener.hpp"

namespace rudp::internal {

struct socket {
    // TODO: There is duplication between the variant and the enum which is state we have to keep in
    // sync and an invariant we must assert throughout the program. std::variant is supposed to give
    // us this for free. Is it worth it?
    enum class type { CREATED, BOUND, LISTENING, CONNECTED } type;
    std::variant<std::monostate, linuxfd_t, std::unique_ptr<listener>, connection_pair> data;

    socket() : type(type::CREATED), data(std::monostate{}) {}
};

extern rudpfd_t next_fd;
extern std::unordered_map<rudpfd_t, socket> g_sockets;

}  // namespace rudp::internal
