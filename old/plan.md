==================
    API LAYER
==================
int socket(void)
    - internally creates a **sockfd**

int bind(int sockfd, struct sockaddr* addr, socklen_t addrlen);
    - forwards

int connect(int sockfd, struct sockaddr* addr, socklen_t addrlen);
    - get the **Connection()** associated with the **sockfd**
    - set the peer on the connection
    - send a syn with **reliably::sendto**
    - transitions the socket into a SYN_SENT
    - blocks until everything is done (condition variables)

int listen(int sockfd, int backlog);
    - sets the state of the **sockfd** to LISTEN

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
    - block waiting for something to come from the established queue

not implemented in MVP:
size_t send(int sockfd, const void* buf, size_t len, int flags);
size_t recv(int sockfd, void* buf, size_t len, int flags);
int close(int sockfd);

- our api layer interacts with the internals only by:
    - creating a listening socket (listen())
    - transitioning the state of a socket to SYN_SENT (connect())
    - pulls from a queue of established connections (accept())

=========================
    EVENT LOOP LAYER
=========================

CLOSED -> ignore

LISTEN (from a listen() call)
    - confirm we received a SYN
    - initialise a new rudp_sock with seq/ack
    - reliably send out the SYNACK
    - transition rudp_sock state to SYN_RCVD

SYN_SENT (after a connect() call)
    - confirm we received a SYNACK
    - reliably send out the ACK
    - transition into ESTABLISHED
    - signal the CV to unblock the ocnnect() call

SYN_RCVD (after being spawned by listen(), expecting an ACK)
    - confirm we received an ACK
    - transition state into ESTABLISHED
    - place our rudp_socket internal fd onto the established queue
    - signal using the CV to the accept() call


ESTABLISHED
    - ignore for now


=========================
    RELIABILITY LAYER
=========================

void reliability.send(fd, FLAG) => marks what we've sent
bool reliability.recv(fd, FLAG) => expects a packet of this flag with correct ack

void reliability.retransmit()

eveyrthing we send out has some expected response:
    - SYN expects SYNACK
    - SYNACK expects ACK
    - ACK expects nothing

reliability.send(fd, FLAG) is oging to store that packet and that expected flag
reliability.recv(fd, FLAG) needs to mark that structure as having received the expected flag

if anything in our data structure has not received their expected flag within some period of time
we will retransmit it
