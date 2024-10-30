#include <stdio.h>
#include <common.h>
#include <dripbox_common.h>
#include <Monitor.h>
#include <Network.h>
#include <stdlib.h>
#include <string.h>
#include <hash_set.h>

struct user_t {
    char *username;
};

#define NOTHING


struct incoming_connection_thread_context_t {
    struct tcp_listener_t *listener;
    hash_table(char*, struct user_t) users_hash_table;
};

static void *incoming_connection_thread(void *arg) {
    const struct incoming_connection_thread_context_t *context = arg;
    const struct tcp_listener_t *listener = context->listener;
    var users = context->users_hash_table;
    struct socket_t client = {};
    static uint8_t server_buffer[1 << 20];

    log(LOG_INFO, "Waiting for connections");
    while (tcp_server_incoming_next(listener, &client, &ipv4_endpoint_empty())) {
        log(LOG_INFO, "Connected to client %s\n", socket_address_to_cstr(client.addr, &default_allocator));
        struct dripbox_payload_header_t header = {};
        int ret = 0;
        ret |= recv(client.sock_fd, &header, sizeof header, 0);
        ret |= recv(client.sock_fd, server_buffer, header.length, 0);
        if (ret < 0) {
            log(LOG_INFO, "%s\n", strerror(errno));
            continue;
        }
        char *msg_type = NULL;
        switch (header.type) {
            case MSG_LOGIN:
                msg_type = "Login";
                username.data = server_buffer;
                username.length = header.length;
                const struct user_t user = {
                    .username = username.data,
                };
                hash_table_insert(users, username.data, user);
                break;
            default:
                log(LOG_ERROR, "Unknown message type: %d\n", header.type);
                continue;
        }

        log(LOG_INFO,
            "Message Received\nVersion: %d\nType: %s\nLength: %d, Body: %.*s\n",
            header.version,
            msg_type,
            header.length,
            (int) username.length, username.data
        );
        close(client.sock_fd);
    }
    return NULL;
}

static int server_main() {
    struct tcp_listener_t listener = tcp_listener_new(AF_INET);
    tcp_listener_bind(&listener, &ipv4_endpoint_new(ip, port));
    tcp_listener_listen(&listener, 10);
    if (listener.last_error != 0) {
        log(LOG_INFO, "%s\n", strerror(listener.last_error));
        return 1;
    }

    pthread_t incoming_connection_thread_id;
    struct incoming_connection_thread_context_t ctx = {
        .listener = &listener,
        .users_hash_table = (void *) hash_table_new(char*, struct user_t, string_hash, string_equals,
                                                    &default_allocator),
    };
    pthread_create(&incoming_connection_thread_id, NULL, incoming_connection_thread, &ctx);

    while (true) {
        sleep(10000);
    }

    return 0;
}
