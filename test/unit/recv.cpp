#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <rudp.hpp>

class RecvUnitTest : public testing::Test {
protected:
    RecvUnitTest() : addr{} {
        auto *addr_in = reinterpret_cast<struct sockaddr_in *>(&addr);
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = INADDR_ANY;
        addr_in->sin_port = htons(1234);
    }
    struct sockaddr addr;
};

TEST_F(RecvUnitTest, BufNull) {
    int clientfd = rudp::socket();
    int serverfd = rudp::socket();

    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), 0);

    socklen_t len = sizeof(addr);
    int accepted_fd = rudp::accept(serverfd, &addr, &len);
    ASSERT_GE(accepted_fd, 0);

    ASSERT_EQ(rudp::recv(accepted_fd, nullptr, 10, 0), -1);
    ASSERT_EQ(errno, EFAULT);
}

TEST_F(RecvUnitTest, ZeroLength) {
    int clientfd = rudp::socket();
    int serverfd = rudp::socket();

    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), 0);

    socklen_t len = sizeof(addr);
    int accepted_fd = rudp::accept(serverfd, &addr, &len);
    ASSERT_GE(accepted_fd, 0);

    char buffer[10];
    ASSERT_EQ(rudp::recv(accepted_fd, buffer, 0, 0), 0);
}

TEST_F(RecvUnitTest, SockDne) {
    char buffer[10];
    ASSERT_EQ(rudp::recv(-1, buffer, sizeof(buffer), 0), -1);
    ASSERT_EQ(errno, EBADF);
}

TEST_F(RecvUnitTest, SocketCreated) {
    int fd = rudp::socket();
    char buffer[10];

    ASSERT_EQ(rudp::recv(fd, buffer, sizeof(buffer), 0), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket in the created state cannot receive data.";
}

TEST_F(RecvUnitTest, SocketBound) {
    int fd = rudp::socket();
    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);

    char buffer[10];

    ASSERT_EQ(rudp::recv(fd, buffer, sizeof(buffer), 0), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket in the bound state cannot receive data.";
}

TEST_F(RecvUnitTest, SocketListening) {
    int fd = rudp::socket();
    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(fd, 1), 0);

    char buffer[10];

    ASSERT_EQ(rudp::recv(fd, buffer, sizeof(buffer), 0), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket in the listening state cannot receive data.";
}

TEST_F(RecvUnitTest, Success) {
    int clientfd = rudp::socket();
    int serverfd = rudp::socket();

    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), 0);

    socklen_t len = sizeof(addr);
    int accepted_fd = rudp::accept(serverfd, &addr, &len);
    ASSERT_GE(accepted_fd, 0);

    const char *message = "hello";
    ASSERT_EQ(rudp::send(clientfd, message, strlen(message), 0),
              static_cast<ssize_t>(strlen(message)));

    char buffer[5]{};
    ssize_t received = rudp::recv(accepted_fd, buffer, sizeof(buffer), 0);

    ASSERT_GT(received, 0);
}
