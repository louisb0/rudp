#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <rudp.hpp>

#include "internal/testing/simulator.hpp"

class SimulationTest : public testing::Test {
protected:
    void SetUp() override {
        rudp::internal::testing::simulator::instance().reset();

        auto *addr_in = reinterpret_cast<struct sockaddr_in *>(&addr);
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = INADDR_ANY;
        addr_in->sin_port = htons(1234);

        serverfd = rudp::socket();
        clientfd = rudp::socket();

        ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
        ASSERT_EQ(rudp::listen(serverfd, 1), 0);
        ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), 0);

        socklen_t len = sizeof(addr);
        accepted_fd = rudp::accept(serverfd, &addr, &len);
        ASSERT_GE(accepted_fd, 0);

        msg_size = 5 * 1024;
        client_data.resize(msg_size);
        server_data.resize(msg_size);

        for (size_t i = 0; i < msg_size; i++) {
            client_data[i] = static_cast<char>('A' + (i % 26));
            server_data[i] = static_cast<char>('a' + (i % 26));
        }
    }

    struct sockaddr addr{};

    int serverfd;
    int clientfd;
    int accepted_fd;
    size_t msg_size;

    std::vector<char> client_data;
    std::vector<char> server_data;

    size_t recv_all(int sock, std::vector<char> &buffer) {
        size_t total = 0;
        while (total < buffer.size()) {
            ssize_t received = rudp::recv(sock, buffer.data() + total, buffer.size() - total, 0);
            EXPECT_GT(received, 0);
            total += static_cast<size_t>(received);
        }
        return total;
    }
};

TEST_F(SimulationTest, PacketLoss30) {
    auto &sim = rudp::internal::testing::simulator::instance();
    sim.drop = 0.3f;

    ASSERT_EQ(rudp::send(clientfd, client_data.data(), client_data.size(), 0),
              static_cast<ssize_t>(client_data.size()));

    std::vector<char> server_received(msg_size);
    size_t total_received = recv_all(accepted_fd, server_received);

    ASSERT_EQ(total_received, msg_size) << "The server must receive all bytes.";
    ASSERT_EQ(memcmp(client_data.data(), server_received.data(), msg_size), 0)
        << "The server must receive the same data sent by the client.";
}

TEST_F(SimulationTest, Latency1000to5000) {
    auto &sim = rudp::internal::testing::simulator::instance();
    sim.min_latency_ms = 1000;
    sim.max_latency_ms = 5000;

    ASSERT_EQ(rudp::send(clientfd, client_data.data(), client_data.size(), 0),
              static_cast<ssize_t>(client_data.size()));

    std::vector<char> server_received(msg_size);
    size_t total_received = recv_all(accepted_fd, server_received);

    ASSERT_EQ(total_received, msg_size) << "The server must receive all bytes.";
    ASSERT_EQ(memcmp(client_data.data(), server_received.data(), msg_size), 0)
        << "The server must receive the same data sent by the client.";
}

TEST_F(SimulationTest, PacketLoss30Latency1000to5000) {
    auto &sim = rudp::internal::testing::simulator::instance();
    sim.drop = 0.3f;
    sim.min_latency_ms = 1000;
    sim.max_latency_ms = 5000;

    ASSERT_EQ(rudp::send(clientfd, client_data.data(), client_data.size(), 0),
              static_cast<ssize_t>(client_data.size()));

    std::vector<char> server_received(msg_size);
    size_t total_received = recv_all(accepted_fd, server_received);

    ASSERT_EQ(total_received, msg_size) << "The server must receive all bytes.";
    ASSERT_EQ(memcmp(client_data.data(), server_received.data(), msg_size), 0)
        << "The server must receive the same data sent by the client.";
}
