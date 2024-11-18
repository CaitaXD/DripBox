#include <stdio.h>
#include <common.h>
#include <dripbox_common.h>
#include <Monitor.h>
#include <Network.h>
#include <string.h>
#include <hash_set.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "string_view.h"

//_Thread_local static char tls_dripbox_path_buffer[PATH_MAX] = {};

struct user_t {
    struct string_view_t username;
    struct socket_t socket;
};

typedef hash_table(char *, struct user_t) user_table_t;

struct incoming_connection_thread_context_t {
    struct tcp_listener_t *listener;
    user_table_t *hash_table;
};

struct payload_t {
    struct dripbox_login_header_t header;
    uint8_t *data;
};

const struct z_string_t g_userdata_dir = (struct z_string_t){
    .data = "./userdata/",
    .length = sizeof "./userdata/" - 1,
};

_Thread_local static uint8_t tls_dripbox_buffer[DRIPBOX_MAX_HEADER_SIZE] = {};

static void dripbox_handle_login(user_table_t *hash_table, struct socket_t client, uint8_t *buffer);

static void dripbox_handle_upload(user_table_t *hash_table, struct user_t user, struct socket_t client,
                                  uint8_t *buffer);

static void dripbox_handle_download(struct user_t user, struct socket_t client, uint8_t *buffer);

static void dripbox_handle_delete(user_table_t hash_table, struct user_t user, struct socket_t client, uint8_t *buffer);

static void handle_massage(user_table_t *hash_table, struct user_t user);

static void *incoming_connections_worker(const void *arg);

static void *client_connections_worker(const void *arg);

static int server_main() {
    signal(SIGPIPE, SIG_IGN);
    struct tcp_listener_t listener = tcp_listener_new(AF_INET);
    tcp_listener_bind(&listener, &ipv4_endpoint_new(ip, port));
    tcp_listener_listen(&listener, SOMAXCONN);
    if (listener.last_error != 0) {
        log(LOG_INFO, "%s\n", strerror(listener.last_error));
        return 1;
    }

    pthread_t incoming_connection_thread_id, client_connection_thread_id;
    var hash_table = hash_table_new(char*, struct user_t, string_hash, string_equals,
                                    &default_allocator);
    struct incoming_connection_thread_context_t ctx = {
        .listener = &listener,
        .hash_table = (user_table_t *) &hash_table,
    };

    pthread_create(&incoming_connection_thread_id, NULL, (void *) incoming_connections_worker, &ctx);
    pthread_create(&client_connection_thread_id, NULL, (void *) client_connections_worker, &ctx);

    while (true) {
        sleep(1);
    }

    return 0;
}

pthread_mutex_t g_users_mutex = PTHREAD_MUTEX_INITIALIZER;

static void dripbox_handle_login(user_table_t *hash_table, struct socket_t client, uint8_t *buffer) {
    const struct dripbox_login_header_t *login_header = (void *) buffer;
    ssize_t got = socket_read_exactly(&client, sizeof(struct dripbox_login_header_t), buffer, 0);
    if (got == 0) { return; }
    if (got < 0) {
        log(LOG_ERROR, "%s\n", strerror(errno));
        return;
    }

    const struct string_view_t username = {
        .data = (char *) tls_dripbox_buffer + sizeof *login_header,
        .length = login_header->length,
    };

    got = socket_read_exactly(&client, sv_deconstruct(username), 0);
    if (got < 0) {
        log(LOG_INFO, "%s\n", strerror(errno));
        return;
    }

    const struct allocator_t *a = hash_set_allocator(*hash_table);

    const struct user_t _user = {
        .username = (struct string_view_t){
            .data = cstr_sv(username, a),
            .length = username.length,
        },
        .socket = client,
    };

    const char *ip = socket_address_to_cstr(client.addr, a);
    hash_table_update(hash_table, ip, _user);

    const struct z_string_t path = path_combine(g_userdata_dir, username);
    struct stat st = {};
    if (stat(g_userdata_dir.data, &st) < 0) {
        mkdir(g_userdata_dir.data, S_IRWXU | S_IRWXG | S_IRWXO);
    }
    if (stat(path.data, &st) < 0) {
        mkdir(path.data, S_IRWXU | S_IRWXG | S_IRWXO);
    }
}

static void dripbox_handle_upload(user_table_t *hash_table, const struct user_t user, struct socket_t client,
                                  uint8_t *buffer) {
    const struct dripbox_upload_header_t *upload_header = (void *) buffer;
    socket_read_exactly(&client, sizeof(struct dripbox_upload_header_t), buffer, 0);
    socket_read_exactly(&client, upload_header->file_name_length, buffer + sizeof *upload_header, 0);
    if (client.error != 0) {
        log(LOG_INFO, "%s\n", strerror(client.error));
        return;
    }

    const struct string_view_t file_name = sv_substr(
        sv_cstr((char *) buffer),
        sizeof *upload_header, upload_header->file_name_length
    );

    const struct string_view_t username = user.username;
    const struct z_string_t path = path_combine(sv_z(g_userdata_dir), username, file_name);
    if (socket_read_to_file(&client, path.data, upload_header->payload_length) != 0) {
        log(LOG_ERROR, "%s\n", strerror(errno));
    }

    const var ht_header = hash_table_header(*hash_table);
    for (int i = 0; i < ht_header->capacity; i++) {
        const var entry = hash_set_entry(*hash_table, i);
        if (HASH_ENTRY_IS_NULL(entry)) { continue; }

        const var kvp = (typeof(*hash_table)) entry->value;
        if (!sv_equals(kvp->value.username, username)) { continue; }
        if (kvp->value.socket.sock_fd == user.socket.sock_fd) { continue; }

        scope(FILE* file = fopen(path.data, "rb"), fclose(file)) {
            if (file == NULL) {
                log(LOG_ERROR, "%s\n", strerror(errno));
                continue;
            }
            struct stat st = {};
            if (fstat(fileno(file), &st) < 0) {
                log(LOG_ERROR, "%s\n", strerror(errno));
                continue;
            }

            const struct dripbox_msg_header_t msg_header = {
                .version = 1,
                .type = MSG_UPLOAD,
            };

            const struct dripbox_upload_header_t new_upload_header = {
                .file_name_length = file_name.length,
                .payload_length = st.st_size,
            };

            for (int i = 0; i < 5; i++) {
                socket_write(&kvp->value.socket, size_and_address(msg_header), 0);
                socket_write(&kvp->value.socket, size_and_address(new_upload_header), 0);
                socket_write_file(&kvp->value.socket, file, new_upload_header.payload_length);
            }
            log(LOG_INFO, "File Uploaded to %s %*s\n", kvp->key, (int)sv_deconstruct(kvp->value.username));
        }
    }
}

void dripbox_handle_download(const struct user_t user, struct socket_t client, uint8_t *buffer) {
    const struct dripbox_download_header_t *download_header = (void *) buffer;

    socket_read_exactly(&client, sizeof(struct dripbox_download_header_t), buffer, 0);
    socket_read_exactly(&client, download_header->file_name_length, buffer + sizeof *download_header, 0);
    if (client.error != 0) {
        log(LOG_ERROR, "%s\n", strerror(client.error));
        return;
    }

    const struct string_view_t file_name = sv_substr(
        sv_cstr((char *) buffer),
        sizeof *download_header, download_header->file_name_length
    );

    const struct string_view_t username = user.username;

    const struct z_string_t path = path_combine(sv_z(g_userdata_dir), username, file_name);

    struct stat st = {};
    if (stat(path.data, &st) < 0) {
        log(LOG_INFO, "Path Invalid %s\n", path.data);
        const struct string_view_t sv_error = sv_cstr(strerror(errno));

        const struct dripbox_msg_header_t msg_header = {
            .version = 1,
            .type = MSG_ERROR,
        };

        const struct dripbox_error_header_t error_header = {
            .error_length = sv_error.length,
        };

        socket_write(&client, size_and_address(msg_header), 0);
        socket_write(&client, size_and_address(error_header), 0);
        socket_write(&client, sv_deconstruct(sv_error), 0);
        if (client.error != 0) {
            log(LOG_ERROR, "%s\n", strerror(client.error));
        }
        return;
    }

    scope(FILE* file = fopen(path.data, "rb"), fclose(file)) {
        if (file == NULL) {
            log(LOG_INFO, "%s\n", strerror(errno));
            break;
        }

        const struct dripbox_msg_header_t msg_header = {
            .version = 1,
            .type = MSG_UPLOAD,
        };

        const struct dripbox_upload_header_t upload_header = {
            .file_name_length = file_name.length,
            .payload_length = st.st_size,
        };

        socket_write(&client, size_and_address(msg_header), 0);
        socket_write(&client, size_and_address(upload_header), 0);
        socket_write(&client, sv_deconstruct(file_name), 0);
        socket_write_file(&client, file, st.st_size);
        if (client.error != 0) {
            log(LOG_ERROR, "%s\n", strerror(errno));
        }
    }
}

void dripbox_handle_delete(user_table_t hash_table, const struct user_t user, struct socket_t client, uint8_t *buffer) {
    const struct dripbox_delete_header_t *download_header = (void *) buffer;

    socket_read_exactly(&client, sizeof(struct dripbox_delete_header_t), buffer, 0);
    socket_read_exactly(&client, download_header->file_name_length, buffer + sizeof *download_header, 0);
    if (client.error != 0) {
        log(LOG_ERROR, "%s\n", strerror(client.error));
        return;
    }

    const struct string_view_t file_name = sv_substr(
        sv_cstr((char *) buffer),
        sizeof *download_header, download_header->file_name_length
    );

    const struct string_view_t username = user.username;
    const struct z_string_t path = path_combine(g_userdata_dir, username, file_name);

    struct stat st = {};
    if (stat(path.data, &st) < 0) {
        log(LOG_INFO, "Path Invalid %s\n", path.data);
        const struct string_view_t sv_error = sv_cstr(strerror(errno));

        const struct dripbox_msg_header_t msg_header = {
            .version = 1,
            .type = MSG_ERROR,
        };

        const struct dripbox_error_header_t error_header = {
            .error_length = sv_error.length,
        };

        socket_write(&client, size_and_address(msg_header), 0);
        socket_write(&client, size_and_address(error_header), 0);
        socket_write(&client, sv_deconstruct(sv_error), 0);
        if (client.error != 0) {
            log(LOG_ERROR, "%s\n", strerror(client.error));
        }
        return;
    }
    if (unlink(path.data) < 0) {
        log(LOG_ERROR, "%s\n", strerror(errno));
        return;
    }

    for (int i = 0; i < hash_table_capacity(hash_table); i++) {
        const var entry = hash_set_entry(hash_table, i);
        if (HASH_ENTRY_IS_NULL(entry)) { continue; }

        const var kvp = (typeof(hash_table)) entry->value;
        if (!sv_equals(kvp->value.username, user.username)) { continue; }
        if (kvp->value.socket.sock_fd == user.socket.sock_fd) { continue; }

        const struct dripbox_msg_header_t msg_header = {
            .version = 1,
            .type = MSG_DELETE,
        };
        const struct dripbox_delete_header_t delete_header = {
            .file_name_length = file_name.length,
        };
        socket_write(&kvp->value.socket, size_and_address(msg_header), 0);
        socket_write(&kvp->value.socket, size_and_address(delete_header), 0);
        socket_write(&kvp->value.socket, sv_deconstruct(file_name), 0);

        log(LOG_INFO, "Sending File Deleted %*s to %s %*s\n",
            (int)sv_deconstruct(file_name), kvp->key,
            (int)sv_deconstruct(kvp->value.username)
            );
    }
    log(LOG_INFO, "File Deleted %s\n", path.data);
}

static void handle_massage(user_table_t *hash_table, const struct user_t user) {
    struct socket_t client = user.socket;
    const struct dripbox_msg_header_t *msg_header = (void *) tls_dripbox_buffer;

    const ssize_t got = socket_read_exactly(&client, size_and_address(*msg_header), 0);
    if (got == 0) { return; }
    if (got < 0) {
        log(LOG_INFO, "%s\n", strerror(errno));
        return;
    }

    uint8_t *buffer = tls_dripbox_buffer + sizeof *msg_header;

    const char *msg_type = msg_type_cstr(msg_header->type);
    switch (msg_header->type) {
    case MSG_NOOP: { return; }
    case MSG_LOGIN: {
        dripbox_handle_login(hash_table, client, buffer);
        break;
    }
    case MSG_UPLOAD: {
        dripbox_handle_upload(hash_table, user, client, buffer);
        break;
    }
    case MSG_DOWNLOAD: {
        dripbox_handle_download(user, client, buffer);
        break;
    }
    case MSG_DELETE: {
        dripbox_handle_delete(*hash_table, user, client, buffer);
        break;
    }
    default:
        log(LOG_INFO, "Invalid Message Type\n");
        break;
    }

    log(LOG_INFO, "Message Received\nVersion: %d\nType: %s\n", msg_header->version, msg_type);
}

void *incoming_connections_worker(const void *arg) {
    const struct incoming_connection_thread_context_t *context = arg;
    const struct tcp_listener_t *listener = context->listener;
    var addr = ipv4_endpoint_empty();

    socket_set_non_blocking(listener->sock_fd);
    bool quit = false;
    while (!quit) {
        struct socket_t client = {};
        while (tcp_server_incoming_next(listener, &client, &addr)) {
            scope(pthread_mutex_lock(&g_users_mutex), pthread_mutex_unlock(&g_users_mutex)) {
                memset(tls_dripbox_buffer, 0, sizeof tls_dripbox_buffer);
                handle_massage(context->hash_table, (struct user_t){.socket = client});
            }
        }
    }
    return NULL;
}

void *client_connections_worker(const void *arg) {
    const struct incoming_connection_thread_context_t *context = arg;
    const struct tcp_listener_t *listener = context->listener;

    socket_set_non_blocking(listener->sock_fd);
    bool quit = false;
    while (!quit) {
        scope(pthread_mutex_lock(&g_users_mutex), pthread_mutex_unlock(&g_users_mutex)) {
            var users = *context->hash_table;
            for (int i = 0; i < hash_table_capacity(users); i++) {
                const var entry = hash_set_entry(users, i);
                if (HASH_ENTRY_IS_NULL(entry)) { continue; }

                const var kvp = *(typeof(users)) entry->value;
                const struct user_t user = kvp.value;
                struct socket_t client = user.socket;
                client.error = 0;

                if (!socket_pending(&client, 0)) {
                    continue;
                }
                handle_massage(&users, user);
            }
        }
    }
    return NULL;
}
