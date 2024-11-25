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

struct socket_address {
    struct sockaddr *sa;
    socklen_t addr_len;
};

struct tcp_listener {
    int sock_fd;
    int last_error;
    struct socket_address *addr;
    int domain;
};

struct socket {
    int sock_fd;
    int error;
    struct socket_address *addr;
};

/// Creates a new tcp listener
/// @param domain The domain to use
/// @return The new tcp listener
NETWORK_API struct tcp_listener tcp_listener_new(int domain);

/// Binds the tcp listener to the given address
/// @remark If error != 0, early returns false
/// @param listener The tcp listener to bind
/// @param addr The address to bind to
/// @return True if the bind was successful, false otherwise
NETWORK_API bool tcp_listener_bind(struct tcp_listener *listener, struct socket_address *addr);

/// Listens for incoming connections on the tcp listener
/// @remark If error != 0, early returns false
/// @param listener The tcp listener to listen on
/// @param backlog Maximum number of pending connections
/// @return True if the listen was successful, false otherwise
NETWORK_API bool tcp_listener_listen(struct tcp_listener *listener, int backlog);

/// Closes the tcp listener
/// @remark If error != 0, early returns false
/// @remark fd is set to -1 if the closed successfully
/// @param listener The tcp listener to close
/// @return True if closed successfully, false otherwise
NETWORK_API bool tcp_listener_close(struct tcp_listener *listener);

/// Accepts a connection on the tcp listener
/// @remark If error != 0, early returns false
/// @param listener The tcp listener to accept on
/// @param addr The address of the client
/// @return The accepted socket
NETWORK_API struct socket tcp_listener_accept(const struct tcp_listener *listener,
                                              struct socket_address *addr);

/// Connects to a remote address
/// @remark If error != 0, early returns false
/// @param client The socket to connect
/// @param addr The address to connect to
/// @return True if the connection was successful, false otherwise
NETWORK_API bool tcp_client_connect(struct socket *client, struct socket_address *addr);

/// Closes the socket
/// @remark If error != 0, early returns false
/// @remark fd is set to -1 if the closed successfully
/// @param s The socket to close
/// @return True if closed successfully, false otherwise
NETWORK_API bool socket_close(struct socket *s);

/// Converts a socket address to a string
/// @param addr The address to convert
/// @param a The allocator to use
/// @return The string representation of the address
NETWORK_API const char *socket_address_to_cstr(const struct socket_address *addr, const struct allocator *a);

/// Converts a socket address to a string in ipv4 format
/// @remark This function is only valid for AF_INET addresses
/// @param addr The address to convert
/// @return The string representation of the address
NETWORK_API const char *ipv4_cstr(const struct socket_address *addr);

/// Waits for the next incoming connection on the tcp listener
/// @param listener The tcp listener to accept on
/// @param client The socket to accept the connection on
/// @param addr The address of the client
/// @return True if an incoming connection was made successfully, false otherwise
NETWORK_API bool tcp_server_incoming_next(const struct tcp_listener *listener, struct socket *client,
                                          struct socket_address *addr);

NETWORK_API struct socket socket_new(int domain, int type, int protocol);

NETWORK_API bool socket_pending(const struct socket *socket, int timeout);

NETWORK_API bool poll_next(size_t len, const struct pollfd fds[static len], int events);

NETWORK_API ssize_t socket_read_exactly(struct socket *s, size_t length, uint8_t buffer[static length],
                                        int flags);

NETWORK_API ssize_t socket_read(struct socket *s, size_t length, uint8_t buffer[static length], int flags);

NETWORK_API ssize_t socket_write(struct socket *s, size_t length, const uint8_t buffer[static length],
                                 int flags);

NETWORK_API ssize_t socket_write_file(struct socket *s, FILE *file, size_t lenght);

NETWORK_API ssize_t socket_redirect_to_file(struct socket *s, const char *path, size_t length);

#define socket_option(fd__, level__, VAL)\
    setsockopt(fd__, level__, SOL_SOCKET, (typeof(VAL)[1]){(VAL)}, sizeof (VAL))

#define socket_set_non_blocking(fd__)\
    fcntl(fd__, F_SETFL, O_NONBLOCK)

#define ipv4_endpoint_new(host_ip__, host_port__)\
    ((struct socket_address) {\
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


struct tcp_listener tcp_listener_new(const int domain) {
    const struct tcp_listener listener = {
        .sock_fd = -1,
        .last_error = 0,
        .domain = domain
    };
    return listener;
}

const char *socket_address_to_cstr(const struct socket_address *addr, const struct allocator *a) {
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

const char *ipv4_cstr(const struct socket_address *addr) {
    assert(addr->sa->sa_family == AF_INET);
    return inet_ntoa(((struct sockaddr_in *) addr->sa)->sin_addr);
}

bool tcp_listener_bind(struct tcp_listener *listener, struct socket_address *addr) {
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

bool tcp_listener_listen(struct tcp_listener *listener, const int backlog) {
    if (listener->last_error != 0) { return false; }
    if (listen(listener->sock_fd, backlog) < 0) {
        listener->last_error = errno;
    }
    return true;
}

struct socket tcp_listener_accept(const struct tcp_listener *listener, struct socket_address *addr) {
    struct socket client = {
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

bool tcp_listener_close(struct tcp_listener *listener) {
    if (listener->last_error != 0) { return false; }
    if (close(listener->sock_fd) < 0) {
        listener->last_error = errno;
        return false;
    }
    listener->sock_fd = -1;
    return true;
}

struct socket socket_new(const int domain, const int type, const int protocol) {
    struct socket client = {
        .sock_fd = -1,
    };
    client.sock_fd = socket(domain, type, protocol);
    if (client.sock_fd < 0) {
        client.error = errno;
        return client;
    }
    return client;
}

bool tcp_client_connect(struct socket *client, struct socket_address *addr) {
    if (client->error != 0) { return false; }
    client->addr = addr;
    if (connect(client->sock_fd, addr->sa, addr->addr_len) < 0) {
        client->error = errno;
        return false;
    }
    return true;
}

bool tcp_server_incoming_next(const struct tcp_listener *listener, struct socket *client,
                              struct socket_address *addr) {
    const int client_socket = accept(listener->sock_fd, addr->sa, &addr->addr_len);
    if (client_socket < 0) {
        client->error = errno;
        return false;
    }
    client->sock_fd = client_socket;
    client->addr = addr;
    return true;
}

bool socket_close(struct socket *s) {
    if (s->error != 0) { return false; }
    if (close(s->sock_fd) < 0) {
        s->error = errno;
        return false;
    }
    s->sock_fd = -1;
    return true;
}

bool socket_pending(const struct socket *socket, const int timeout) {
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

ssize_t socket_read_exactly(struct socket *s, const size_t length, uint8_t buffer[static length],
                            const int flags) {
    size_t left_to_read = length;
    while (left_to_read > 0) {
        const ssize_t recvd = recv(s->sock_fd, buffer, left_to_read, flags);
        if (recvd == 0) {
            socket_close(s);
            return length - left_to_read;
        }
        if (recvd < 0) {
            s->error = errno;
            return -1;
        }
        buffer += recvd;
        left_to_read -= recvd;
    }
    return length;
}

ssize_t socket_read(struct socket *s, const size_t length, uint8_t buffer[static length], const int flags) {
    if (s->error != 0) { return -1; }
    const ssize_t got = recv(s->sock_fd, buffer, length, flags);
    if (got == 0) {
        if(length > 0) { socket_close(s); }
        return 0;
    }
    if (got < 0) {
        s->error = errno;
        return -1;
    }
    return got;
}

ssize_t socket_write(struct socket *s, const size_t length, const uint8_t buffer[static length],
                     const int flags) {
    if (s->error != 0) { return -1; }
    const ssize_t sent = send(s->sock_fd, buffer, length, flags);
    if (sent == 0) {
        if(length > 0) { socket_close(s); }
        return 0;
    }
    if (sent < 0) {
        s->error = errno;
        return -1;
    }
    return sent;
}

ssize_t socket_write_file(struct socket *s, FILE *file, size_t lenght) {
    if (s->error != 0) { return -1; }
    const int fd = fileno(file);
    if (fd < 0) { return 0; }

    off_t offset = 0;
    while (lenght > 0) {
        //fseek(file, offset, SEEK_SET);
        const ssize_t result = sendfile(s->sock_fd, fd, &offset, lenght);
        if (result == 0) {
            socket_close(s);
            return offset;
        }
        if (result < 0) {
            s->error = errno;
            return -1;
        }
        lenght -= result;
    }
    return offset;
}

ssize_t socket_redirect_to_file(struct socket *s, const char *path, const size_t length) {
    if (s->error != 0) { return -1; }

    size_t left_to_read = length;
    uint8_t buffer[1024] = {};
    scope(FILE *file = fopen(path, "wb"), file && fclose(file)) {
        if (file == NULL) {
            continue;
        }
        while (left_to_read > 0) {
            const size_t len = min(length, sizeof buffer);
            const ssize_t got = socket_read(s, len, buffer, 0);

            if (got == 0) { break; }
            if (got < 0) {
                break;
            }
            left_to_read -= got;
            if (fwrite(buffer, sizeof(uint8_t), got, file) < 0) {
                break;
            }
        }
    }
    return length - left_to_read;
}

static void *_socket_read_struct_impl(struct socket *socket, const size_t length, uint8_t buffer[length],
                                      const int flags) {
    socket_read_exactly(socket, length, buffer, flags);
    return buffer;
}

#define socket_read_struct(socket__, type__, flags__) \
    (*(type__*)_socket_read_struct_impl(socket__, sizeof(type__), (void*)((type__[1]){}), (flags__)))

#define socket_read_array(socket__, type__, length__, flags__) \
    ({\
        struct { size_t length; type__ data[(length__)]; } _array;\
        _array.length = sizeof _array.data;\
        socket_read_exactly(socket__, _array.length, _array.data, flags__);\
        _array;\
    }).data

#define socket_write_struct(socket__, struct__, flags__) \
    ({\
        var __struct = (struct__);\
        socket_write(socket__, size_and_address(__struct), flags__);\
    })

#endif //NETWORK_H
