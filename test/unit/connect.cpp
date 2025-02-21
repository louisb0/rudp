#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <rudp.hpp>

class ConnectTest : public testing::Test {
protected:
    ConnectTest() : addr{} {
        auto *addr_in = reinterpret_cast<struct sockaddr_in *>(&addr);
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = INADDR_ANY;
        addr_in->sin_port = htons(1234);  // non-zero port for connect
    }
    struct sockaddr addr;
};

TEST_F(ConnectTest, AddrNull) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::connect(fd, nullptr, 0), -1);
    ASSERT_EQ(errno, EINVAL);
}

TEST_F(ConnectTest, AddrlenNotInet) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::connect(fd, &addr, sizeof(addr) - 1), -1);
    ASSERT_EQ(errno, EINVAL);
}

TEST_F(ConnectTest, AddrFamilyNotInet) {
    int fd = rudp::socket();

    addr.sa_family = AF_UNIX;

    ASSERT_EQ(rudp::connect(fd, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EAFNOSUPPORT);
}

TEST_F(ConnectTest, SocketDne) {
    ASSERT_EQ(rudp::connect(-1, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EBADF);
}

TEST_F(ConnectTest, SocketListening) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(fd, 1), 0);

    ASSERT_EQ(rudp::connect(fd, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A listening socket cannot initiate connections.";
}

TEST_F(ConnectTest, SocketConnected) {
    int serverfd = rudp::socket();
    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);

    int clientfd = rudp::socket();
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket cannot connect twice.";
}

TEST_F(ConnectTest, SuccessFromCreated) {
    int serverfd = rudp::socket();
    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);

    int clientfd = rudp::socket();
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), 0);
}

TEST_F(ConnectTest, SuccessFromBound) {
    int serverfd = rudp::socket();
    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);

    int clientfd = rudp::socket();
    struct sockaddr client_addr{};
    auto *client_addr_in = reinterpret_cast<struct sockaddr_in *>(&client_addr);
    client_addr_in->sin_family = AF_INET;
    client_addr_in->sin_addr.s_addr = INADDR_ANY;
    client_addr_in->sin_port = 0;
    ASSERT_EQ(rudp::bind(clientfd, &client_addr, sizeof(client_addr)), 0);

    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), 0);
}
