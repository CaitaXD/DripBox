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
#include <coroutine.h>

#define NETWORK_DEBUG_INFO
#ifndef NETWORK_API
#   define NETWORK_API static inline
#endif

struct error_info {
    int code;
};

#define SETSOCK_ERROR(err_info__) ({\
    var _einfo = &(err_info__);\
    _einfo->code = errno;\
    _einfo;\
})

struct socket_address {
    struct sockaddr *sa;
    socklen_t addr_len;
};

struct socket {
    int sock_fd;
    struct socket_address *addr;
    struct error_info error;
};

struct tcp_listener {
    union {
        struct socket as_socket;
        struct {
            int sock_fd;
            struct socket_address *addr;
            struct error_info error;
        };
    };
};

/// Creates a new tcp listener
NETWORK_API struct tcp_listener tcp_listener_new();

/// Binds the tcp listener to the given address
/// @remark If error != 0, early returns false
/// @param listener The tcp listener to bind
/// @param domain The domain to use
/// @param addr The address to bind to
/// @return True if the bind was successful, false otherwise
NETWORK_API bool tcp_listener_bind(struct tcp_listener *listener, int domain, struct socket_address *addr);

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
NETWORK_API char *socket_address_to_cstr(const struct socket_address *addr, struct allocator *a);

/// Converts a socket address to a string in ipv4 format
/// @remark This function is only valid for AF_INET addresses
/// @param addr The address to convert
/// @param a The allocator to use
/// @return The string representation of the address
NETWORK_API char *ipv4_cstr(const struct socket_address *addr, struct allocator *a);

/// Waits for the next incoming connection on the tcp listener
/// @param listener The tcp listener to accept on
/// @param client The socket to accept the connection on
/// @param addr The address of the client
/// @return True if an incoming connection was made successfully, false otherwise
NETWORK_API bool tcp_server_incoming_next(const struct tcp_listener *listener, struct socket *client,
                                          struct socket_address *addr);

NETWORK_API bool socket_bind(struct socket *socket, struct socket_address *addr);

NETWORK_API bool socket_listen(struct socket *socket, int backlog);

NETWORK_API bool socket_connect(struct socket *socket, const struct socket_address *remote);

NETWORK_API struct socket socket_accept(struct socket *socket, struct socket_address *remote);

NETWORK_API struct socket socket_new(void);

NETWORK_API bool socket_open(struct socket *s, int domain, int type, int protocol);

NETWORK_API bool socket_pending_read(const struct socket *socket, int timeout);

NETWORK_API bool socket_pending_write(const struct socket *socket, int timeout);

NETWORK_API bool socket_pending_event(const struct socket *socket, int events, int timeout);

NETWORK_API int socket_poll(const struct socket *socket, int events, int timeout);

NETWORK_API ssize_t socket_read_exactly(struct socket *s, size_t length, uint8_t buffer[static length],
                                        int flags);

NETWORK_API ssize_t socket_read(struct socket *s, size_t length, uint8_t buffer[static length], int flags);

NETWORK_API ssize_t socket_write(struct socket *s, size_t length, const uint8_t buffer[static length],
                                 int flags);

NETWORK_API ssize_t socket_write_to(struct socket *s, size_t length, const uint8_t buffer[static length],
                                    int flags,
                                    const struct socket_address *addr);

NETWORK_API ssize_t socket_read_from(struct socket *s, size_t length, uint8_t buffer[static length],
                                     int flags,
                                     struct socket_address *addr);

NETWORK_API ssize_t socket_write_file(struct socket *s, FILE *file, size_t lenght);

NETWORK_API ssize_t socket_redirect_to_file(struct socket *s, const char *path, size_t length);

NETWORK_API bool socket_blocking(struct socket *socket, bool blocking);

NETWORK_API bool socket_reuse_address(struct socket *socket, bool reuse);

NETWORK_API bool socket_join_multicast_group(struct socket *socket, struct ip_mreq group);

NETWORK_API void* socket_accept_async(struct coroutine *co, struct socket *sock, struct socket_address *addr);

NETWORK_API void* socket_connect_async(struct coroutine *co, struct socket *sock, struct socket_address *addr);

NETWORK_API void socket_adress_set_port(const struct socket_address *addr, uint16_t port);

NETWORK_API void socket_adress_set_in_addr(const struct socket_address *addr, uint32_t ip);

#define socket_option2(sock__, level__, optname__, val__)\
    ({\
        var s = (sock__);\
        var _val = (val__);\
        int ret = setsockopt(s->sock_fd, (level__), (optname__), &_val, sizeof _val);\
        if (ret < 0) { s->error.code = errno; }\
        ret == 0;\
    })

#define socket_option1(sock__, optname__, val__) \
    socket_option2((sock__), SOL_SOCKET, (optname__), (val__))

#define socket_option(...) \
    MACRO_SELECT4(__VA_ARGS__, socket_option2, socket_option1, socket_option1, socket_option1)(__VA_ARGS__)

#define ipv4_endpoint(host_ip__, host_port__)\
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

#define sockaddr_in(host_ip__, host_port__)\
    ((struct sockaddr_in) {\
        .sin_family = AF_INET,\
        .sin_port = htons(host_port__),\
        .sin_addr = {\
            .s_addr = htonl(host_ip__)\
        }\
    })

#define ipv4_address(x, y, z, w) (((uint32_t)(x)<<24)|((uint32_t)(y)<<16)|((uint32_t)(z)<<8)|(uint32_t)(w))

#define socket_read_struct(socket__, type__, flags__) \
    (*(type__*)_socket_read_struct_impl(socket__, sizeof(type__), (void*)((type__[1]){}), (flags__)))

#define socket_read_array(socket__, type__, length__, flags__) \
({\
    struct { size_t length; type__ data[(length__)]; } _array;\
    _array.length = (length__);\
    size_t _sz = sizeof _array.data;\
    socket_read_exactly(socket__, _sz, (uint8_t*)_array.data, flags__);\
    _array;\
}).data

#define socket_write_struct(socket__, struct__, flags__) \
({\
    var _struct = (struct__);\
    (socket_write)((socket__), size_and_address(_struct), (flags__));\
})

#define socket_write_struct_to(socket__, struct__, remote__, flags__) \
({\
    var _struct = (struct__);\
    socket_write_to((socket__), size_and_address(_struct), (flags__), (remote__));\
})

#define socket_read_struct_from(socket__, type__, remote__, flags__) \
({\
    type__ _struct = {};\
    socket_read_from((socket__), size_and_address(_struct), (flags__), (remote__));\
    _struct;\
})

bool socket_blocking(struct socket *socket, const bool blocking) {
    const int flags = fcntl(socket->sock_fd, F_GETFL);
    if (flags < 0) {
        socket->error.code = errno;
        return false;
    }
    const int blocking_flag = blocking ? ~O_NONBLOCK : O_NONBLOCK;
    if ((flags & blocking_flag) != 0) {
        return true;
    }
    const int ret = fcntl(socket->sock_fd, F_SETFL, (flags & blocking_flag));
    if (ret < 0) {
        socket->error.code = errno;
        return false;
    }
    return true;
}

bool socket_reuse_address(struct socket *socket, const bool reuse) {
    const int val = reuse ? 1 : 0;
    return socket_option(socket, SOL_SOCKET, SO_REUSEADDR, val);
}

bool socket_join_multicast_group(struct socket *socket, const struct ip_mreq group) {
    return socket_option(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, group);
}

struct tcp_listener tcp_listener_new() {
    const struct tcp_listener listener = {
        .sock_fd = -1,
        .error.code = 0,
    };
    return listener;
}

char *socket_address_to_cstr(const struct socket_address *addr, struct allocator *a) {
    const int U16_MAX_DIGITS = 5; // ceil(log10(pow(2, 16)))
    switch (addr->sa->sa_family) {
    case AF_INET: {
        const int MAX_STRING_LENGTH = INET_ADDRSTRLEN + U16_MAX_DIGITS + 1;
        const uint16_t port = ntohs(((struct sockaddr_in *) addr->sa)->sin_port);
        char *buffer = allocator_alloc(a, MAX_STRING_LENGTH);
        inet_ntop(AF_INET, &((struct sockaddr_in *) addr->sa)->sin_addr, buffer, INET_ADDRSTRLEN);
        sprintf(buffer + strlen(buffer), ":%d", port);
        return buffer;
    }
    case AF_INET6: {
        const int MAX_STRING_LENGTH = INET6_ADDRSTRLEN + U16_MAX_DIGITS + 1;
        const uint16_t port = ntohs(((struct sockaddr_in6 *) addr->sa)->sin6_port);
        char *buffer = allocator_alloc(a, MAX_STRING_LENGTH);
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

char *ipv4_cstr(const struct socket_address *addr, struct allocator *a) {
    assert(addr->sa->sa_family == AF_INET);
    return allocator_copy(a, inet_ntoa(((struct sockaddr_in *) addr->sa)->sin_addr), 16);
}

bool (tcp_listener_bind)(struct tcp_listener *listener, const int domain, struct socket_address *addr) {
    if (listener->error.code != 0) { return false; }
    listener->addr = addr;

    listener->sock_fd = socket(domain, SOCK_STREAM, IPPROTO_TCP);
    if (listener->sock_fd < 0) {
        SETSOCK_ERROR(listener->error);
        return false;
    }

    if (bind(listener->sock_fd, addr->sa, addr->addr_len) < 0) {
        listener->error.code = errno;
        return false;
    }
    return true;
}

bool (tcp_listener_listen)(struct tcp_listener *listener, const int backlog) {
    if (listener->error.code != 0) { return false; }
    if (listen(listener->sock_fd, backlog) < 0) {
        SETSOCK_ERROR(listener->error);
    }
    return true;
}

struct socket (tcp_listener_accept)(const struct tcp_listener *listener, struct socket_address *addr) {
    struct socket s = {
        .error.code = listener->error.code,
        .addr = addr,
    };
    if (listener->error.code != 0) { return s; }
    s.sock_fd = accept(listener->sock_fd, s.addr->sa, &s.addr->addr_len);
    if (s.sock_fd < 0) {
        SETSOCK_ERROR(s.error);
        return s;
    }
    return s;
}

bool (tcp_listener_close)(struct tcp_listener *listener) {
    if (listener->error.code != 0) { return false; }
    if (close(listener->sock_fd) < 0) {
        SETSOCK_ERROR(listener->error);
        return false;
    }
    listener->sock_fd = -1;
    return true;
}

struct socket socket_new(void) {
    const struct socket s = {
        .sock_fd = -1,
    };
    return s;
}

bool socket_open(struct socket *s, const int domain, const int type, const int protocol) {
    s->sock_fd = socket(domain, type, protocol);
    if (s->sock_fd < 0) {
        SETSOCK_ERROR(s->error);
        return false;
    }
    return true;
}

bool (tcp_client_connect)(struct socket *client, struct socket_address *addr) {
    if (client->error.code != 0) { return false; }
    client->addr = addr;
    if (connect(client->sock_fd, addr->sa, addr->addr_len) < 0) {
        SETSOCK_ERROR(client->error);
        return false;
    }
    return true;
}

bool (tcp_server_incoming_next)(const struct tcp_listener *listener, struct socket *client,
                              struct socket_address *addr) {
    const int client_socket = accept(listener->sock_fd, addr->sa, &addr->addr_len);
    if (client_socket < 0) {
        SETSOCK_ERROR(client->error);
        return false;
    }
    client->sock_fd = client_socket;
    client->addr = addr;
    return true;
}

bool (socket_close)(struct socket *s) {
    if (s->error.code != 0) { return false; }
    if (close(s->sock_fd) < 0) {
        SETSOCK_ERROR(s->error);
        return false;
    }
    s->sock_fd = -1;
    return true;
}

bool (socket_bind)(struct socket *socket, struct socket_address *addr) {
    if (socket->error.code != 0) { return false; }
    if (bind(socket->sock_fd, addr->sa, addr->addr_len) < 0) {
        socket->error.code = errno;
        SETSOCK_ERROR(socket->error);
        return false;
    }
    socket->addr = addr;
    return true;
}

bool (socket_listen)(struct socket *socket, const int backlog) {
    if (socket->error.code != 0) { return false; }
    if (listen(socket->sock_fd, backlog) < 0) {
        SETSOCK_ERROR(socket->error);
        return false;
    }
    return true;
}

bool (socket_connect)(struct socket *socket, const struct socket_address *remote) {
    if (socket->error.code != 0) { return false; }
    if (connect(socket->sock_fd, remote->sa, remote->addr_len) < 0) {
        SETSOCK_ERROR(socket->error);
        return false;
    }
    return true;
}

struct socket (socket_accept)(struct socket *socket, struct socket_address *remote) {
    struct socket client = {
        .error.code = socket->error.code,
        .addr = remote,
    };
    if (socket->error.code != 0) { return client; }
    socket->sock_fd = accept(socket->sock_fd, remote->sa, &remote->addr_len);
    if (socket->sock_fd < 0) {
        SETSOCK_ERROR(socket->error);
        SETSOCK_ERROR(client.error);
    }
    return client;
}

bool socket_pending_read(const struct socket *socket, const int timeout) {
    if (socket->error.code != 0) { return false; }

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

bool socket_pending_write(const struct socket *socket, const int timeout) {
    if (socket->error.code != 0) { return false; }

    struct pollfd pfd = {
        .fd = socket->sock_fd,
        .events = POLLOUT,
    };
    const int event_count = poll(&pfd, 1, timeout);
    if (event_count > 0) {
        return pfd.revents & POLLOUT;
    }
    return false;
}

bool socket_pending_event(const struct socket *socket, const int events, const int timeout) {
    if (socket->error.code != 0) { return false; }

    struct pollfd pfd = {
        .fd = socket->sock_fd,
        .events = events,
    };
    const int event_count = poll(&pfd, 1, timeout);
    if (event_count > 0) {
        return pfd.revents & events;
    }
    return false;
}

int socket_poll(const struct socket *socket, const int events, const int timeout) {
    if (socket->error.code != 0) { return -1; }
    struct pollfd pfd = {
        .fd = socket->sock_fd,
        .events = events,
    };
    poll(&pfd, 1, timeout);
    return pfd.revents;
}

ssize_t (socket_read_exactly)(struct socket *s, const size_t length, uint8_t buffer[static length],
                            const int flags) {
    size_t left_to_read = length;
    while (left_to_read > 0) {
        const ssize_t recvd = recv(s->sock_fd, buffer, left_to_read, flags);
        if (recvd == 0) {
            socket_close(s);
            return length - left_to_read;
        }
        if (recvd < 0) {
            SETSOCK_ERROR(s->error);
            return -1;
        }
        buffer += recvd;
        left_to_read -= recvd;
    }
    assert(left_to_read == 0 && "Dawg this aint your bytes to read");
    return length;
}

ssize_t (socket_read)(struct socket *s, const size_t length, uint8_t buffer[static length],
                    const int flags) {
    if (s->error.code != 0) { return -1; }
    const ssize_t got = recv(s->sock_fd, buffer, length, flags);
    if (got == 0) {
        if(length > 0) { (socket_close)(s); }
        return 0;
    }
    if (got < 0) {
        SETSOCK_ERROR(s->error);
        return -1;
    }
    return got;
}

ssize_t (socket_write)(struct socket *s, const size_t length, const uint8_t buffer[static length],
                     const int flags) {
    if (s->error.code != 0) { return -1; }
    const ssize_t sent = send(s->sock_fd, buffer, length, flags);
    if (sent == 0) {
        if(length > 0) { (socket_close)(s); }
        return 0;
    }
    if (sent < 0) {
        SETSOCK_ERROR(s->error);
        return -1;
    }
    return sent;
}

ssize_t (socket_write_file)(struct socket *s, FILE *file, size_t lenght) {
    if (s->error.code != 0) { return -1; }
    const int fd = fileno(file);
    if (fd < 0) { return 0; }

    off_t offset = 0;
    while (lenght > 0) {
        //fseek(file, offset, SEEK_SET);
        const ssize_t result = sendfile(s->sock_fd, fd, &offset, lenght);
        if (result == 0) {
            (socket_close)(s);
            return offset;
        }
        if (result < 0) {
            SETSOCK_ERROR(s->error);
            return -1;
        }
        lenght -= result;
    }
    return offset;
}

ssize_t (socket_redirect_to_file)(struct socket *s, const char *path, const size_t length) {
    if (s->error.code != 0) { return -1; }

    size_t left_to_read = length;
    uint8_t buffer[1024] = {};
    scope(FILE *file = fopen(path, "wb"), file && fclose(file)) {
        if (file == NULL) return -1;

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

ssize_t (socket_write_to)(struct socket *s, const size_t length, const uint8_t buffer[static length],
                        const int flags,
                        const struct socket_address *addr) {
    if (s->error.code != 0) { return -1; }

    const ssize_t sent = sendto(s->sock_fd, buffer, length, flags, addr->sa, addr->addr_len);
    if (sent == 0) {
        if(length > 0) { (socket_close)(s); }
        return 0;
    }
    if (sent < 0) {
        SETSOCK_ERROR(s->error);
        return -1;
    }
    return sent;
}

ssize_t (socket_read_from)(struct socket *s, const size_t length, uint8_t buffer[static length],
                         const int flags, struct socket_address *addr) {
    if (s->error.code != 0) { return -1; }

    const ssize_t got = recvfrom(s->sock_fd, buffer, length, flags, addr->sa, &addr->addr_len);
    if (got == 0) {
        if(length > 0) { (socket_close)(s); }
        return 0;
    }
    if (got < 0) {
        SETSOCK_ERROR(s->error);
        return -1;
    }
    return got;
}

void* socket_accept_async(struct coroutine *co, struct socket *sock, struct socket_address *addr) {
    struct socket client;
    COROUTINE(co, socket_accept_async, sock, addr, client) {
        client = socket_accept(sock, addr);
        while (sock->error.code == EAGAIN) {
            co_yield(co);
            sock->error.code = 0;
            client = socket_accept(sock, addr);
        }
    }
    return co->return_value;
}

void* socket_connect_async(struct coroutine *co, struct socket *sock, struct socket_address *addr) {
    bool connected = false;
    COROUTINE(co, socket_connect_async, sock, addr, connected) {
        socket_connect(sock, addr);
        while (sock->error.code == EAGAIN) {
            co_yield(co);
            sock->error.code = 0;
            socket_connect(sock, addr);
        }
        connected = sock->error.code == 0;
        co_return(co, connected);
    }
    return co->return_value;
}

static void socket_adress_set_port(const struct socket_address *addr, const uint16_t port) {
    switch (addr->sa->sa_family) {
    case AF_INET: {
        ((struct sockaddr_in *) addr->sa)->sin_port = htons(port);
        break;
    }
    case AF_INET6: {
        ((struct sockaddr_in6 *) addr->sa)->sin6_port = htons(port);
        break;
    }
    default:
        assert(false && "I don't know you, and i don't care to know you");
        break;
    }
}

static void socket_adress_set_in_addr(const struct socket_address *addr, const uint32_t ip) {
    assert(addr->sa->sa_family == AF_INET);
    ((struct sockaddr_in *) addr->sa)->sin_addr.s_addr = htonl(ip);
}

static in_addr_t socket_address_get_in_addr(const struct socket_address *addr) {
    assert(addr->sa->sa_family == AF_INET);
    return ((struct sockaddr_in *) addr->sa)->sin_addr.s_addr;
}

static void *_socket_read_struct_impl(struct socket *socket, const size_t length, uint8_t buffer[length],
                                      const int flags) {
    (socket_read_exactly)(socket, length, buffer, flags);
    return buffer;
}

#endif //NETWORK_H
