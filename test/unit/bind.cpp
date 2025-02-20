#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cerrno>
#include <rudp.hpp>

class BindTest : public testing::Test {
protected:
    BindTest() : addr{} {
        auto *addr_in = reinterpret_cast<struct sockaddr_in *>(&addr);
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = INADDR_ANY;
        addr_in->sin_port = 0;
    }
    struct sockaddr addr;
};

TEST_F(BindTest, AddrNull) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, nullptr, 0), -1);
    ASSERT_EQ(errno, EINVAL);
}

TEST_F(BindTest, AddrSizeNotInet) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr) - 1), -1);
    ASSERT_EQ(errno, EINVAL);
}

TEST_F(BindTest, AddrFamilyNotInet) {
    int fd = rudp::socket();

    addr.sa_family = AF_UNIX;

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EAFNOSUPPORT);
}

TEST_F(BindTest, SocketDne) {
    ASSERT_EQ(rudp::bind(-1, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EBADF);
}

TEST_F(BindTest, SocketBound) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket cannot be bound twice.";
}

TEST_F(BindTest, SocketListening) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(fd, 1), 0);

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket cannot be bound once listening.";
}

TEST_F(BindTest, SocketConnected) {
    reinterpret_cast<struct sockaddr_in *>(&addr)->sin_port = htons(1234);

    int serverfd = rudp::socket();
    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);

    int clientfd = rudp::socket();
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(struct sockaddr_in)), 0);

    ASSERT_EQ(rudp::bind(clientfd, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket cannot be bound once connected.";
}

// TODO: We should instead test the preserve_errno() utility function separately.
TEST_F(BindTest, ForwardsErrno) {
    int fd = rudp::socket();

    auto *addr_in = reinterpret_cast<struct sockaddr_in *>(&addr);
    addr_in->sin_port = htons(80);  // requires root

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), -1);
    ASSERT_EQ(errno, EACCES) << "errno must be forwarded to the caller.";
}

TEST_F(BindTest, Success) {
    int fd = rudp::socket();

    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);
}
