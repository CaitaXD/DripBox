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
    int last_error;
    struct socket_address_t *addr;
};

NETWORK_API struct tcp_listener_t tcp_listener_new(int domain);

NETWORK_API bool tcp_listener_bind(struct tcp_listener_t *listener, struct socket_address_t *addr);

NETWORK_API bool tcp_listener_listen(struct tcp_listener_t *listener, int backlog);

NETWORK_API bool tcp_listener_close(struct tcp_listener_t *listener);

NETWORK_API struct socket_t tcp_listener_accept(const struct tcp_listener_t *listener,
                                                struct socket_address_t *addr);

NETWORK_API struct socket_t socket_new(int domain);

NETWORK_API bool tcp_client_connect(struct socket_t *client, struct socket_address_t *addr);

NETWORK_API bool socket_close(struct socket_t *s);

NETWORK_API const char *
socket_address_to_cstr(const struct socket_address_t *addr, const struct allocator_t *allocator);

NETWORK_API struct socket_stream_t socket_stream(struct socket_t *socket, int flags);

NETWORK_API bool tcp_server_incoming_next(const struct tcp_listener_t *listener, struct socket_t *client,
                                          struct socket_address_t *addr);

#define socket_option(fd__, level__, VAL)\
    setsockopt(fd__, level__, SOL_SOCKET, (typeof(VAL)[1]){(VAL)}, sizeof (VAL))

#define socket_non_blocking(fd__)\
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

const char *socket_address_to_cstr(const struct socket_address_t *addr, const struct allocator_t *allocator) {
    switch (addr->sa->sa_family) {
        case AF_INET: {
            const uint16_t port = ntohs(((struct sockaddr_in *) addr->sa)->sin_port);
            char *buffer = alloc(allocator, INET_ADDRSTRLEN + 6);
            inet_ntop(AF_INET, &((struct sockaddr_in *) addr->sa)->sin_addr, buffer, INET_ADDRSTRLEN);
            sprintf(buffer + strlen(buffer), ":%d", port);
            return buffer;
        }
        case AF_INET6: {
            const uint16_t port = ntohs(((struct sockaddr_in6 *) addr->sa)->sin6_port);
            char *buffer = alloc(allocator, INET6_ADDRSTRLEN + 6);
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *) addr->sa)->sin6_addr, buffer, INET6_ADDRSTRLEN);
            sprintf(buffer + strlen(buffer), ":%d", port);
            return buffer;
        }
        default:
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
        .last_error = listener->last_error,
        .addr = addr,
    };
    if (listener->last_error != 0) { return client; }
    client.sock_fd = accept(listener->sock_fd, client.addr->sa, &client.addr->addr_len);
    if (client.sock_fd < 0) {
        client.last_error = errno;
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

struct socket_t socket_new(const int domain) {
    struct socket_t client = {
        .sock_fd = -1,
    };
    client.sock_fd = socket(domain, SOCK_STREAM, IPPROTO_TCP);
    if (client.sock_fd < 0) {
        client.last_error = errno;
        return client;
    }
    return client;
}

bool tcp_client_connect(struct socket_t *client, struct socket_address_t *addr) {
    if (client->last_error != 0) { return false; }
    client->addr = addr;
    if (connect(client->sock_fd, addr->sa, addr->addr_len) < 0) {
        client->last_error = errno;
        return false;
    }
    return true;
}

bool socket_close(struct socket_t *s) {
    if (s->last_error != 0) { return false; }
    if (close(s->sock_fd) < 0) {
        s->last_error = errno;
        return false;
    }
    s->sock_fd = -1;
    return true;
}

struct stream_t {
    ssize_t (*write)(struct stream_t *stream, size_t length, uint8_t data[length]);

    ssize_t (*read)(struct stream_t *stream, size_t length, uint8_t data[length]);
};

struct socket_stream_t {
    struct stream_t stream;
    struct socket_t *socket;
    int flags;
};

static ssize_t socket_write(struct stream_t *stream, const size_t length, uint8_t data[length]) {
    const var socket_stream = container_of(stream, struct socket_stream_t, stream);
    const var socket = socket_stream->socket;
    if (socket->last_error != 0) {
        return -1;
    }
    return send(socket->sock_fd, data, length, socket_stream->flags);
}

static ssize_t socket_read(struct stream_t *stream, const size_t length, uint8_t data[length]) {
    const var socket_stream = container_of(stream, struct socket_stream_t, stream);
    const var socket = socket_stream->socket;
    if (socket->last_error != 0) {
        return -1;
    }
    return recv(socket->sock_fd, data, length, socket_stream->flags);
}

struct socket_stream_t socket_stream(struct socket_t *socket, const int flags) {
    const struct socket_stream_t stream = {
        .stream = {
            .write = socket_write,
            .read = socket_read,
        },
        .socket = socket,
        .flags = flags,
    };
    return stream;
}

bool tcp_server_incoming_next(const struct tcp_listener_t *listener, struct socket_t *client,
                              struct socket_address_t *addr) {
    const int client_socket = accept(listener->sock_fd, addr->sa, &addr->addr_len);
    if (client_socket < 0) {
        client->last_error = errno;
        return false;
    }
    client->sock_fd = client_socket;
    client->addr = addr;
    return true;
}

#endif //NETWORK_H
