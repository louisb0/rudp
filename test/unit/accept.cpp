#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <rudp.hpp>

class AcceptTest : public testing::Test {
protected:
    AcceptTest() : addr{} {
        auto *addr_in = reinterpret_cast<struct sockaddr_in *>(&addr);
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = INADDR_ANY;
        addr_in->sin_port = 0;
    }
    struct sockaddr addr;
};

TEST_F(AcceptTest, FilloutAddrlenNull) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(fd, 1), 0);

    ASSERT_EQ(rudp::accept(fd, &addr, nullptr), -1);
    ASSERT_EQ(errno, EINVAL);
}

TEST_F(AcceptTest, FilloutAddrlenNotInet) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(fd, 1), 0);

    socklen_t bad_len = sizeof(addr) - 1;
    ASSERT_EQ(rudp::accept(fd, &addr, &bad_len), -1);
    ASSERT_EQ(errno, EINVAL);
}

TEST_F(AcceptTest, SocketDne) {
    socklen_t len = sizeof(addr);
    ASSERT_EQ(rudp::accept(-1, &addr, &len), -1);
    ASSERT_EQ(errno, EBADF);
}

TEST_F(AcceptTest, SocketCreated) {
    int fd = rudp::socket();

    socklen_t len = sizeof(addr);
    ASSERT_EQ(rudp::accept(fd, &addr, &len), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket in the created state cannot accept connections.";
}

TEST_F(AcceptTest, SocketBound) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);

    socklen_t len = sizeof(addr);
    ASSERT_EQ(rudp::accept(fd, &addr, &len), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket in the bound state cannot accept connections.";
}

TEST_F(AcceptTest, SocketConnected) {
    reinterpret_cast<struct sockaddr_in *>(&addr)->sin_port = htons(1234);

    int serverfd = rudp::socket();
    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);

    int clientfd = rudp::socket();
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), 0);

    socklen_t len = sizeof(addr);
    ASSERT_EQ(rudp::accept(clientfd, &addr, &len), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket in the connected state cannot accept connections.";
}

TEST_F(AcceptTest, Success) {
    reinterpret_cast<struct sockaddr_in *>(&addr)->sin_port = htons(1234);

    int serverfd = rudp::socket();
    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);

    int clientfd = rudp::socket();
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), 0);

    // Accept a connection.
    socklen_t len = sizeof(addr);
    int accepted_fd = rudp::accept(serverfd, &addr, &len);
    ASSERT_GE(accepted_fd, 0);

    // Verify the peer address was filled.
    auto *accepted_addr = reinterpret_cast<struct sockaddr_in *>(&addr);
    ASSERT_EQ(accepted_addr->sin_family, AF_INET);
    ASSERT_NE(accepted_addr->sin_port, 0);  // client port is unknown
}
