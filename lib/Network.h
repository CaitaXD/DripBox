#ifndef NETWORK_H
#define NETWORK_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <poll.h>
#include <common.h>
#include <stddef.h>
#include <sys/sendfile.h>
#include <Allocator.h>

#ifndef NETWORK_API
#   define NETWORK_API static inline
#endif

struct socket_address_t {
    struct sockaddr *sa;
    socklen_t addr_len;
};

struct tcp_listener_t {
    int sock_fd;
    int last_error;
    struct socket_address_t *addr;
    int domain;
};

struct socket_t {
    int sock_fd;
    int error;
    struct socket_address_t *addr;
};

NETWORK_API struct tcp_listener_t tcp_listener_new(int domain);

NETWORK_API bool tcp_listener_bind(struct tcp_listener_t *listener, struct socket_address_t *addr);

NETWORK_API bool tcp_listener_listen(struct tcp_listener_t *listener, int backlog);

NETWORK_API bool tcp_listener_close(struct tcp_listener_t *listener);

NETWORK_API struct socket_t tcp_listener_accept(const struct tcp_listener_t *listener,
                                                struct socket_address_t *addr);


NETWORK_API bool tcp_client_connect(struct socket_t *client, struct socket_address_t *addr);

NETWORK_API bool socket_close(struct socket_t *s);

NETWORK_API const char *socket_address_to_cstr(const struct socket_address_t *addr, const struct allocator_t *a);

NETWORK_API bool tcp_server_incoming_next(const struct tcp_listener_t *listener, struct socket_t *client,
                                          struct socket_address_t *addr);

NETWORK_API struct socket_t socket_new(int domain, int type, int protocol);

NETWORK_API bool socket_pending(const struct socket_t *socket, int timeout);

NETWORK_API bool poll_next(size_t len, const struct pollfd fds[static len], int events);

NETWORK_API ssize_t socket_read_exactly(struct socket_t *socket, size_t length, uint8_t buffer[static length],
                                        int flags);

NETWORK_API ssize_t socket_read(struct socket_t *socket, size_t length, uint8_t buffer[static length], int flags);

NETWORK_API ssize_t socket_write(struct socket_t *socket, size_t length, uint8_t buffer[static length], int flags);

NETWORK_API ssize_t socket_read_file(struct socket_t *socket, FILE *file, size_t lenght);

#define socket_option(fd__, level__, VAL)\
    setsockopt(fd__, level__, SOL_SOCKET, (typeof(VAL)[1]){(VAL)}, sizeof (VAL))

#define socket_set_non_blocking(fd__)\
    fcntl(fd__, F_SETFL, O_NONBLOCK)

#define ipv4_endpoint_new(host_ip__, host_port__)\
    ((struct socket_address_t) {\
        .sa = (struct sockaddr *) &(struct sockaddr_in) {\
            .sin_family = AF_INET,\
            .sin_port = htons(host_port__),\
            .sin_addr = {\
                .s_addr = htonl(host_ip__)\
            }\
        },\
        .addr_len = sizeof(struct sockaddr_in)\
    })

#define ipv4_endpoint_empty() ipv4_endpoint_new(0, 0)


struct tcp_listener_t tcp_listener_new(const int domain) {
    const struct tcp_listener_t listener = {
        .sock_fd = -1,
        .last_error = 0,
        .domain = domain
    };
    return listener;
}

const char *socket_address_to_cstr(const struct socket_address_t *addr, const struct allocator_t *a) {
    switch (addr->sa->sa_family) {
    case AF_INET: {
        const uint16_t port = ntohs(((struct sockaddr_in *) addr->sa)->sin_port);
        char *buffer = allocator_alloc(a, INET_ADDRSTRLEN + 6);
        inet_ntop(AF_INET, &((struct sockaddr_in *) addr->sa)->sin_addr, buffer, INET_ADDRSTRLEN);
        sprintf(buffer + strlen(buffer), ":%d", port);
        return buffer;
    }
    case AF_INET6: {
        const uint16_t port = ntohs(((struct sockaddr_in6 *) addr->sa)->sin6_port);
        char *buffer = allocator_alloc(a, INET6_ADDRSTRLEN + 6);
        inet_ntop(AF_INET6, &((struct sockaddr_in6 *) addr->sa)->sin6_addr, buffer, INET6_ADDRSTRLEN);
        sprintf(buffer + strlen(buffer), ":%d", port);
        return buffer;
    }
    default:
        assert(false && "I don't know you, and i don't care to know you");
        break;
    }
    unreachable();
}

bool tcp_listener_bind(struct tcp_listener_t *listener, struct socket_address_t *addr) {
    if (listener->last_error != 0) { return false; }
    const int domain = listener->domain;
    listener->sock_fd = socket(domain, SOCK_STREAM, IPPROTO_TCP);
    if (listener->sock_fd < 0) {
        listener->last_error = errno;
        return false;
    }

    listener->addr = addr;
    if (bind(listener->sock_fd, addr->sa, addr->addr_len) < 0) {
        listener->last_error = errno;
    }
    return true;
}

bool tcp_listener_listen(struct tcp_listener_t *listener, const int backlog) {
    if (listener->last_error != 0) { return false; }
    if (listen(listener->sock_fd, backlog) < 0) {
        listener->last_error = errno;
    }
    return true;
}

struct socket_t tcp_listener_accept(const struct tcp_listener_t *listener, struct socket_address_t *addr) {
    struct socket_t client = {
        .error = listener->last_error,
        .addr = addr,
    };
    if (listener->last_error != 0) { return client; }
    client.sock_fd = accept(listener->sock_fd, client.addr->sa, &client.addr->addr_len);
    if (client.sock_fd < 0) {
        client.error = errno;
        return client;
    }
    return client;
}

bool tcp_listener_close(struct tcp_listener_t *listener) {
    if (listener->last_error != 0) { return false; }
    if (close(listener->sock_fd) < 0) {
        listener->last_error = errno;
        return false;
    }
    listener->sock_fd = -1;
    return true;
}

struct socket_t socket_new(const int domain, const int type, const int protocol) {
    struct socket_t client = {
        .sock_fd = -1,
    };
    client.sock_fd = socket(domain, type, protocol);
    if (client.sock_fd < 0) {
        client.error = errno;
        return client;
    }
    return client;
}

bool tcp_client_connect(struct socket_t *client, struct socket_address_t *addr) {
    if (client->error != 0) { return false; }
    client->addr = addr;
    if (connect(client->sock_fd, addr->sa, addr->addr_len) < 0) {
        client->error = errno;
        return false;
    }
    return true;
}

bool tcp_server_incoming_next(const struct tcp_listener_t *listener, struct socket_t *client,
                              struct socket_address_t *addr) {
    const int client_socket = accept(listener->sock_fd, addr->sa, &addr->addr_len);
    if (client_socket < 0) {
        client->error = errno;
        return false;
    }
    client->sock_fd = client_socket;
    client->addr = addr;
    return true;
}

bool socket_close(struct socket_t *s) {
    if (s->error != 0) { return false; }
    if (close(s->sock_fd) < 0) {
        s->error = errno;
        return false;
    }
    s->sock_fd = -1;
    return true;
}

bool socket_pending(const struct socket_t *socket, const int timeout) {
    if (socket->error != 0) { return false; }

    struct pollfd pfd = {
        .fd = socket->sock_fd,
        .events = POLLIN,
    };
    const int event_count = poll(&pfd, 1, timeout);
    if (event_count > 0) {
        return pfd.revents & POLLIN;
    }
    return false;
}

bool poll_next(const size_t len, const struct pollfd fds[], const int events) {
    for (size_t i = 0; i < len; i++) {
        if (fds[i].revents & events) {
            return true;
        }
    }
    return false;
}

ssize_t socket_read_exactly(struct socket_t *socket, const size_t length, uint8_t buffer[static length],
                            const int flags) {
    ptrdiff_t left_to_read = length;
    while (left_to_read > 0) {
        const ssize_t recvd = recv(socket->sock_fd, buffer, left_to_read, flags);
        if (recvd == 0) {
            return 0;
        }
        if (recvd < 0) {
            socket->error = errno;
            return -1;
        }
        buffer += recvd;
        left_to_read -= recvd;
    }
    return length;
}

ssize_t socket_read(struct socket_t *socket, const size_t length, uint8_t buffer[static length], const int flags) {
    if (socket->error != 0) { return -1; }
    if (recv(socket->sock_fd, buffer, length, flags) < 0) {
        socket->error = errno;
        return -1;
    }
    return length;
}

ssize_t socket_write(struct socket_t *socket, const size_t length, uint8_t buffer[static length], const int flags) {
    if (socket->error != 0) { return -1; }
    if (send(socket->sock_fd, buffer, length, flags) < 0) {
        socket->error = errno;
        return -1;
    }
    return length;
}

ssize_t socket_read_file(struct socket_t *socket, FILE *file, const size_t lenght) {
    if (socket->error != 0) { return -1; }
    const int fd = fileno(file);
    if (fd < 0) { return 0; }

    ptrdiff_t left_to_read = lenght;
    off_t offset = 0;
    while (left_to_read > 0) {
        fseek(file, offset, SEEK_SET);
        const ssize_t result = sendfile(socket->sock_fd, fd, &offset, left_to_read);
        if (result < 0) {
            socket->error = errno;
            return -1;
        }
        left_to_read -= result;
    }

    return lenght;
}

#endif //NETWORK_H
