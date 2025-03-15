#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <rudp.hpp>

class ListenUnitTest : public testing::Test {
protected:
    ListenUnitTest() : addr{} {
        auto *addr_in = reinterpret_cast<struct sockaddr_in *>(&addr);
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = INADDR_ANY;
        addr_in->sin_port = 0;
    }
    struct sockaddr addr;
};

TEST_F(ListenUnitTest, BacklogSaturates) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);

    ASSERT_EQ(rudp::listen(fd, SOMAXCONN + 1), 0);
}

TEST_F(ListenUnitTest, BacklogInvalid) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);

    ASSERT_EQ(rudp::listen(fd, 0), -1);
    ASSERT_EQ(errno, EINVAL);
}

TEST_F(ListenUnitTest, SocketDne) {
    ASSERT_EQ(rudp::listen(-1, 1), -1);
    ASSERT_EQ(errno, EBADF);
}

TEST_F(ListenUnitTest, SocketCreated) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::listen(fd, 1), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket must be bound before it can listen.";
}

TEST_F(ListenUnitTest, SocketListening) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);

    ASSERT_EQ(rudp::listen(fd, 1), 0);
    ASSERT_EQ(rudp::listen(fd, 1), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket cannot listen twice.";
}

TEST_F(ListenUnitTest, SocketConnected) {
    reinterpret_cast<struct sockaddr_in *>(&addr)->sin_port = htons(1234);

    int serverfd = rudp::socket();
    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);

    int clientfd = rudp::socket();
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), 0);

    ASSERT_EQ(rudp::listen(clientfd, 1), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket cannot listen once connected.";
}

TEST_F(ListenUnitTest, Success) {
    int fd = rudp::socket();
    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(fd, 1), 0);
}
