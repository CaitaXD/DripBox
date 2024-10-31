#include <stdio.h>
#include <common.h>
#include <dripbox_common.h>
#include <Monitor.h>
#include <Network.h>
#include <stdlib.h>
#include <string.h>
#include <hash_set.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>

struct user_t {
    char *username;
    struct socket_t socket;
};

struct incoming_connection_thread_context_t {
    struct tcp_listener_t *listener;
    hash_table(char*, struct user_t) users_hash_table;
};

struct payload_t {
    struct dripbox_payload_header_t header;
    uint8_t *data;
};

const char *msg_type_cstr(const enum msg_type msg_type) {
    const char *str = NULL;
    switch (msg_type) {
    case MSG_UPLOAD:
        str = "Upload";
        break;
    case MSG_LOGIN:
        str = "Login";
        break;
    case MSG_NOOP:
        str = "NOOP";
        break;
    default:
        str = "INVALID MESSAGE";
        break;
    }
    return str;
}

static void handle_massage(void **users_hash_table,
                           uint8_t *buffer,
                           const struct socket_t client) {
    struct dripbox_payload_header_t header = {};
    int recvd = recv(client.sock_fd, &header, sizeof header, 0);
    if (recvd < 0) {
        log(LOG_INFO, "%s\n", strerror(errno));
        return;
    }

    recvd = recv(client.sock_fd, buffer, header.length, 0);
    if (recvd < 0) {
        log(LOG_INFO, "%s\n", strerror(errno));
        return;
    }

    buffer[recvd] = 0;
    const char *msg_type = msg_type_cstr(header.type);
    switch (header.type) {
    case MSG_NOOP: { return; }
    case MSG_LOGIN: {
        username.data = (char *) buffer;
        username.length = header.length;
        const struct user_t user = {
            .username = username.data,
            .socket = client,
        };

        hash_table(char*, struct user_t) ht = *users_hash_table;
        hash_table_insert(ht, username.data, user);
        *users_hash_table = ht;

        const char userdata[] = "./userdata/";
        char path_buffer[PATH_MAX] = {};
        const int len = snprintf(path_buffer, PATH_MAX, "./userdata/%*s", (int) username.length, username.data);
        path_buffer[len] = 0;
        struct stat st = {};
        if (stat(userdata, &st) < 0) {
            mkdir(userdata, S_IRWXU | S_IRWXG | S_IRWXO);
        }
        if (stat(path_buffer, &st) < 0) {
            mkdir(path_buffer, S_IRWXU | S_IRWXG | S_IRWXO);
        }
        break;
    }
    case MSG_UPLOAD: {
        printf("%*s", (int) header.length, (char *) buffer);
        break;
    }
    default: assert(false && "Invalid Message Type");
    }
    log(LOG_INFO,
        "Message Received\nVersion: %d\nType: %s\nLength: %zu, Body: %.*s\n",
        header.version,
        msg_type,
        header.length,
        (int) username.length, username.data
    );
}

static void *incoming_connection_thread(void *arg) {
    const struct incoming_connection_thread_context_t *context = arg;
    const struct tcp_listener_t *listener = context->listener;
    var users = context->users_hash_table;
    var addr = ipv4_endpoint_empty();
    static uint8_t buffer[1 << 20] = {};

    socket_non_blocking(listener->sock_fd);
    const volatile bool quit = false;
    while (!quit) {
        var header = hash_table_header(users);
        for (int i = 0; i < hash_table_capacity(users); i++) {
            if (i >= hash_table_length(users)) { break; }
            const var entry = hash_set_entry(users, i);
            if (HASH_ENTRY_IS_NULL(entry)) { continue; }
            const var kvp = *(typeof(users)) entry->value;

            const struct user_t user = kvp.value;
            const struct socket_t client = user.socket;
            if (client.sock_fd == -1) { continue; }

            handle_massage((void*)&users, buffer, client);
        }

        struct socket_t client = {};
        while (tcp_server_incoming_next(listener, &client, &addr)) {
            handle_massage((void*)&users, buffer, client);
        }
    }
    return NULL;
}

static int server_main() {
    struct tcp_listener_t listener = tcp_listener_new(AF_INET);
    tcp_listener_bind(&listener, &ipv4_endpoint_new(ip, port));
    tcp_listener_listen(&listener, SOMAXCONN);
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
        sleep(1);
    }

    return 0;
}
