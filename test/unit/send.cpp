#include <gtest/gtest.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>

#include <rudp.hpp>

class SendUnitTest : public testing::Test {
protected:
    SendUnitTest() : addr{} {
        auto *addr_in = reinterpret_cast<struct sockaddr_in *>(&addr);
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = INADDR_ANY;
        addr_in->sin_port = htons(1234);
    }
    struct sockaddr addr;
};

TEST_F(SendUnitTest, BufNull) {
    int clientfd = rudp::socket();
    int serverfd = rudp::socket();

    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), 0);

    socklen_t len = sizeof(addr);
    int accepted_fd = rudp::accept(serverfd, &addr, &len);
    ASSERT_GE(accepted_fd, 0);

    ASSERT_EQ(rudp::send(clientfd, nullptr, 10, 0), -1);
    ASSERT_EQ(errno, EFAULT);
}

TEST_F(SendUnitTest, ZeroLength) {
    int clientfd = rudp::socket();
    int serverfd = rudp::socket();

    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), 0);

    socklen_t len = sizeof(addr);
    int accepted_fd = rudp::accept(serverfd, &addr, &len);
    ASSERT_GE(accepted_fd, 0);

    const char *message = "test";
    ASSERT_EQ(rudp::send(clientfd, message, 0, 0), 0);
}

TEST_F(SendUnitTest, SockDne) {
    const char *message = "hello";

    ASSERT_EQ(rudp::send(-1, message, strlen(message), 0), -1);
    ASSERT_EQ(errno, EBADF);
}

TEST_F(SendUnitTest, SocketCreated) {
    int fd = rudp::socket();
    const char *message = "hello";

    ASSERT_EQ(rudp::send(fd, message, strlen(message), 0), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket in the created state cannot send data.";
}

TEST_F(SendUnitTest, SocketBound) {
    int fd = rudp::socket();
    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);

    const char *message = "hello";

    ASSERT_EQ(rudp::send(fd, message, strlen(message), 0), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket in the bound state cannot send data.";
}

TEST_F(SendUnitTest, SocketListening) {
    int fd = rudp::socket();
    ASSERT_EQ(rudp::bind(fd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(fd, 1), 0);

    const char *message = "hello";

    ASSERT_EQ(rudp::send(fd, message, strlen(message), 0), -1);
    ASSERT_EQ(errno, EOPNOTSUPP) << "A socket in the listening state cannot send data.";
}

TEST_F(SendUnitTest, Success) {
    int clientfd = rudp::socket();
    int serverfd = rudp::socket();

    ASSERT_EQ(rudp::bind(serverfd, &addr, sizeof(addr)), 0);
    ASSERT_EQ(rudp::listen(serverfd, 1), 0);
    ASSERT_EQ(rudp::connect(clientfd, &addr, sizeof(addr)), 0);

    socklen_t len = sizeof(addr);
    int accepted_fd = rudp::accept(serverfd, &addr, &len);
    ASSERT_GE(accepted_fd, 0);

    const char *message = "hello";
    ssize_t sent = rudp::send(clientfd, message, strlen(message), 0);
    ASSERT_EQ(sent, static_cast<ssize_t>(strlen(message)));
}
