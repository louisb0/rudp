#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <rudp.hpp>

class ListenTest : public testing::Test {
protected:
    ListenTest() : addr{} {
        auto *addr_in = reinterpret_cast<struct sockaddr_in *>(&addr);
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = INADDR_ANY;
        addr_in->sin_port = 0;
    }
    struct sockaddr addr;
};

TEST_F(ListenTest, BacklogSaturates) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);

    ASSERT_EQ(rudp::listen(fd, SOMAXCONN + 1), 0);
}

TEST_F(ListenTest, BacklogInvalid) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);

    ASSERT_EQ(rudp::listen(fd, 0), -1);
    ASSERT_EQ(errno, EINVAL);
}

TEST_F(ListenTest, SocketDne) {
    ASSERT_EQ(rudp::listen(-1, 1), -1);
    ASSERT_EQ(errno, EBADF);
}

TEST_F(ListenTest, SocketCreated) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::listen(fd, 1), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket must be bound before it can listen.";
}

TEST_F(ListenTest, SocketListening) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);

    ASSERT_EQ(rudp::listen(fd, 1), 0);
    ASSERT_EQ(rudp::listen(fd, 1), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket cannot listen twice.";
}

TEST_F(ListenTest, SocketConnected) {
    reinterpret_cast<struct sockaddr_in *>(&addr)->sin_port = htons(1234);

    int serverfd = rudp::socket();
    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);

    int clientfd = rudp::socket();
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), 0);

    ASSERT_EQ(rudp::listen(clientfd, 1), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket cannot listen once connected.";
}

TEST_F(ListenTest, Success) {
    int fd = rudp::socket();
    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(fd, 1), 0);
}
