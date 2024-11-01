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
    struct dripbox_login_header_t header;
    uint8_t *data;
};

typedef int errno_t;

static errno_t upload_file(const size_t length, uint8_t data[length], const char *path) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return errno;
    }
    size_t sent = 0;
    while (sent < length) {
        const int ret = fwrite(data + sent, sizeof(uint8_t), length - sent, file);
        if (ret < 0) {
            return errno;
        }
        sent += ret;
    }
    const int ret = fclose(file);
    if (ret != 0) {
        return errno;
    }
    return 0;
}

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
                           const struct user_t user) {
    const struct socket_t client = user.socket;
    const struct dripbox_generic_header_t *header = (void *) buffer;
    int recvd = recv(client.sock_fd, buffer, sizeof *header, MSG_DONTWAIT);
    if (recvd < 0) {
        if (errno != EAGAIN) {
            log(LOG_INFO, "%s\n", strerror(errno));
        } else if (errno == EWOULDBLOCK) {
            recv(client.sock_fd, buffer, sizeof *header, 0);
        } else {
            return;
        }
    }

    buffer[recvd] = 0;
    const char *msg_type = msg_type_cstr(header->type);
    switch (header->type) {
    case MSG_NOOP: { return; }
    case MSG_LOGIN: {
        const struct dripbox_login_header_t *login_header = (void *) buffer;
        const size_t diff = sizeof *login_header - sizeof *header;
        assert(diff > 0 && "???");

        recvd = recv(client.sock_fd, buffer + sizeof *header, diff, 0);
        if (recvd < 0) {
            log(LOG_INFO, "%s\n", strerror(errno));
            return;
        }

        recvd = recv(client.sock_fd, buffer + sizeof *login_header, login_header->length, 0);
        if (recvd < 0) {
            log(LOG_INFO, "%s\n", strerror(errno));
            return;
        }

        const struct string_view_t username = {
            .data = alloc_copy(hash_set_allocator(*users_hash_table), buffer + sizeof *login_header,
                               login_header->length),
            .length = login_header->length,
        };
        const struct user_t user = {
            .username = username.data,
            .socket = client,
        };

        hash_table_insert((hash_table(char*, struct user_t)*) users_hash_table, username.data, user);

        const char userdata[] = "./userdata/";
        char path_buffer[PATH_MAX] = {};
        const int len = snprintf(path_buffer, PATH_MAX, "./userdata/%*s", (int) username.length, username.data);
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
        const struct dripbox_upload_header_t *upload_header = (void *) buffer;
        const size_t diff = sizeof *upload_header - sizeof *header;
        int ret = recv(client.sock_fd, buffer + sizeof *header, diff, 0);
        if (ret < 0) {
            log(LOG_INFO, "%s\n", strerror(errno));
            return;
        }
        recvd += ret;

        ret = recv(client.sock_fd, buffer + sizeof *upload_header, upload_header->file_name_length, 0);
        if (ret < 0) {
            log(LOG_INFO, "%s\n", strerror(errno));
            return;
        }
        recvd += ret;

        ret = recv(client.sock_fd, buffer + sizeof *upload_header + upload_header->file_name_length,
                   upload_header->payload_length, 0);
        if (ret < 0) {
            log(LOG_INFO, "%s\n", strerror(errno));
            return;
        }
        recvd += ret;

        const struct string_view_t file_name = sv_substr(sv_from_cstr((char *) buffer), sizeof *upload_header,
                                                         upload_header->file_name_length);
        uint8_t *file_data = buffer + sizeof *upload_header + upload_header->file_name_length;

        const struct string_view_t sv_username = sv_from_cstr(user.username);
        const size_t len = strlen("./userdata/") + sv_username.length + strlen("/") + file_name.length + 1;
        char path_buffer[PATH_MAX] = {};
        ret = snprintf(path_buffer,
                       len,
                       "./userdata/%*s/%*s",
                       (int) sv_username.length, sv_username.data,
                       (int) file_name.length, file_name.data);
        path_buffer[len] = 0;
        if (ret < 0) {
            log(LOG_ERROR, "%s", strerror(errno));
            return;
        }
        if (upload_file(upload_header->payload_length, file_data, path_buffer) != 0) {
            log(LOG_INFO, "%s\n", strerror(errno));
            return;
        }

        break;
    }
    default:
        log(LOG_INFO, "Invalid Message Type\n");
        break;
    }

    log(LOG_INFO, "Message Received\nVersion: %d\nType: %s\n", header->version, msg_type);
}

static uint8_t server_buffer[1 << 20] = {};

static void *incoming_connection_thread(void *arg) {
    const struct incoming_connection_thread_context_t *context = arg;
    const struct tcp_listener_t *listener = context->listener;
    var users = context->users_hash_table;
    var addr = ipv4_endpoint_empty();

    socket_non_blocking(listener->sock_fd);
    const volatile bool quit = false;
    while (!quit) {
        for (int i = 0; i < hash_table_capacity(users); i++) {
            if (i >= hash_table_length(users)) { break; }
            const var entry = hash_set_entry(users, i);
            if (HASH_ENTRY_IS_NULL(entry)) { continue; }
            const var kvp = *(typeof(users)) entry->value;

            const struct user_t user = kvp.value;
            const struct socket_t client = user.socket;
            if (client.sock_fd == -1) { continue; }

            handle_massage((void **) &users, server_buffer, user);
        }

        struct socket_t client = {};
        while (tcp_server_incoming_next(listener, &client, &addr)) {
            handle_massage((void **) &users, server_buffer, (struct user_t){.socket = client});
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
