#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <rudp.hpp>

class BindUnitTest : public testing::Test {
protected:
    BindUnitTest() : addr{} {
        auto *addr_in = reinterpret_cast<struct sockaddr_in *>(&addr);
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = INADDR_ANY;
        addr_in->sin_port = 0;
    }
    struct sockaddr addr;
};

TEST_F(BindUnitTest, AddrNull) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, nullptr, 0), -1);
    ASSERT_EQ(errno, EFAULT);
}

TEST_F(BindUnitTest, AddrlenNotInet) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr) - 1), -1);
    ASSERT_EQ(errno, EINVAL);
}

TEST_F(BindUnitTest, AddrFamilyNotInet) {
    int fd = rudp::socket();

    addr.sa_family = AF_UNIX;

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EAFNOSUPPORT);
}

TEST_F(BindUnitTest, SocketDne) {
    ASSERT_EQ(rudp::bind(-1, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EBADF);
}

TEST_F(BindUnitTest, SocketBound) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket cannot be bound twice.";
}

TEST_F(BindUnitTest, SocketListening) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(fd, 1), 0);

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket cannot be bound once listening.";
}

TEST_F(BindUnitTest, SocketConnected) {
    reinterpret_cast<struct sockaddr_in *>(&addr)->sin_port = htons(1234);

    int serverfd = rudp::socket();
    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);

    int clientfd = rudp::socket();
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(struct sockaddr_in)), 0);

    ASSERT_EQ(rudp::bind(clientfd, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket cannot be bound once connected.";
}

TEST_F(BindUnitTest, ForwardsErrno) {
    int fd = rudp::socket();

    reinterpret_cast<struct sockaddr_in *>(&addr)->sin_port = htons(80);  // requires root

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EACCES) << "errno must be forwarded to the caller.";
}

TEST_F(BindUnitTest, Success) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);
}
