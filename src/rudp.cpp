#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "internal/common.hpp"

#define MAX_CONNECTIONS 1024

#define RUDP_SYN 1
#define RUDP_SYNACK 2
#define RUDP_ACK 3

struct rudp_header {
    u16 seq_num;
    u16 ack_num;
    u8 flags;
} __attribute__((packed));

struct rudp_connection {
    int sockfd;
    u16 seq_num;
    u16 ack_num;
    bool connected;
};

static rudp_connection connections[MAX_CONNECTIONS];

int rudp_socket(void) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    rudp_connection* conn = &connections[sockfd];
    conn->sockfd = sockfd;
    conn->seq_num = 0;
    conn->ack_num = 0;

    return sockfd;
}

int rudp_bind(int sockfd, struct sockaddr* addr, socklen_t addrlen) {
    return bind(sockfd, addr, addrlen);
}

int rudp_connect(int sockfd, struct sockaddr* addr, socklen_t addrlen) {
    rudp_connection* conn = &connections[sockfd];

    rudp_header syn;
    syn.seq_num = conn->seq_num++;
    syn.ack_num = conn->ack_num;
    syn.flags = RUDP_SYN;

    if (sendto(sockfd, &syn, sizeof(syn), 0, addr, addrlen) <= 0) {
        return -1;
    }

    // NOTE: This is an MVP, but, anybody could send a SYN-ACK at the perfect time and hijack this
    // connection.
    rudp_header synack;
    socklen_t len = addrlen;
    if (recvfrom(sockfd, &synack, sizeof(rudp_header), 0, addr, &len) <= 0) {
        return -1;
    }

    if (synack.flags != RUDP_SYNACK || synack.ack_num != conn->seq_num) {
        return -1;
    }

    conn->ack_num = synack.seq_num + 1;

    rudp_header ack;
    ack.seq_num = conn->seq_num++;
    ack.ack_num = conn->ack_num;
    ack.flags = RUDP_ACK;

    if (sendto(sockfd, &ack, sizeof(ack), 0, addr, addrlen) <= 0) {
        return -1;
    }

    conn->connected = true;
    return 1;
}

int rudp_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    rudp_connection* conn = &connections[sockfd];

    rudp_header syn;
    if (recvfrom(sockfd, &syn, sizeof(syn), 0, addr, addrlen) <= 0) {
        return -1;
    }

    if (syn.flags != RUDP_SYN) {
        return -1;
    }

    conn->ack_num = syn.seq_num + 1;

    rudp_header synack;
    synack.seq_num = conn->seq_num++;
    synack.ack_num = conn->ack_num;
    synack.flags = RUDP_SYNACK;

    if (sendto(sockfd, &synack, sizeof(synack), 0, addr, *addrlen) <= 0) {
        return -1;
    }

    rudp_header ack;
    if (recvfrom(sockfd, &ack, sizeof(ack), 0, addr, addrlen) <= 0) {
        return -1;
    }

    if (ack.flags != RUDP_ACK || ack.ack_num != conn->seq_num) {
        return -1;
    }

    conn->connected = true;
    return 1;
}

int rudp_close(int sockfd) {
    connections[sockfd].connected = false;
    return close(sockfd);
}

int rudp_listen(int sockfd, int backlog);
size_t rudp_send(int sockfd, const void* buf, size_t len, int flags);
size_t rudp_recv(int sockfd, void* buf, size_t len, int flags);
