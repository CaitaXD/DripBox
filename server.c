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
    struct string_view_t username;
    struct socket_t socket;
};

struct incoming_connection_thread_context_t {
    struct tcp_listener_t *listener;
    hash_table(char*, struct user_t) *hash_table;
};

struct payload_t {
    struct dripbox_login_header_t header;
    uint8_t *data;
};

const struct string_view_t g_userdata_dir = (struct string_view_t){
    .data = "./userdata/",
    .length = sizeof "./userdata/" - 1,
};

_Thread_local static uint8_t tls_dripbox_buffer[DRIPBOX_MAX_HEADER_SIZE] = {};

static void dripbox_handle_login(void **hash_table,
                                 struct socket_t client);

static void dripbox_handle_upload(struct user_t user,
                                  struct socket_t client);

static void dripbox_handle_download(struct user_t user,
                                    struct socket_t client);

static void handle_massage(void **users_hash_table,
                           struct user_t user);

static void *incoming_connections_worker(void *arg);

static void *client_connections_worker(void *arg);

static int server_main() {
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
        .hash_table = (void *) &hash_table,
    };

    pthread_create(&incoming_connection_thread_id, NULL, incoming_connections_worker, &ctx);
    pthread_create(&client_connection_thread_id, NULL, client_connections_worker, &ctx);

    while (true) {
        sleep(1);
    }

    return 0;
}

pthread_mutex_t g_users_mutex = PTHREAD_MUTEX_INITIALIZER;

static void dripbox_handle_login(void **hash_table, const struct socket_t client) {
    const struct dripbox_login_header_t *login_header = (void *) tls_dripbox_buffer;
    const size_t diff = sizeof *login_header - sizeof(struct dripbox_generic_header_t);
    assert(diff > 0 && "???");

    int got = recv(client.sock_fd, tls_dripbox_buffer + sizeof(struct dripbox_generic_header_t), diff, 0);
    if (got < 0) {
        log(LOG_INFO, "%s\n", strerror(errno));
        return;
    }

    const struct string_view_t username = {
        .data = (char *) tls_dripbox_buffer + sizeof *login_header,
        .length = login_header->length,
    };

    got = recv(client.sock_fd, username.data, username.length, 0);
    if (got < 0) {
        log(LOG_INFO, "%s\n", strerror(errno));
        return;
    }

    scope(pthread_mutex_lock(&g_users_mutex), pthread_mutex_unlock(&g_users_mutex)) {
        const var users = (hash_table(char*, struct user_t)*) hash_table;
        const struct allocator_t *a = hash_set_allocator(*users);

        const struct user_t _user = {
            .username = (struct string_view_t){
                .data = sv_to_cstr(username, a),
                .length = username.length,
            },
            .socket = client,
        };

        hash_table_update(users, _user.username.data, _user);
    }

    const size_t len = g_userdata_dir.length + username.length + 1;
    const int ret = snprintf(tls_dripbox_path_buffer, len, "./userdata/%*s", (int) username.length, username.data);
    tls_dripbox_path_buffer[len] = 0;
    if (ret < 0) {
        log(LOG_ERROR, "%s", strerror(errno));
        return;
    }

    struct stat st = {};
    if (stat(g_userdata_dir.data, &st) < 0) {
        mkdir(g_userdata_dir.data, S_IRWXU | S_IRWXG | S_IRWXO);
    }
    if (stat(tls_dripbox_path_buffer, &st) < 0) {
        mkdir(tls_dripbox_path_buffer, S_IRWXU | S_IRWXG | S_IRWXO);
    }
}

static void dripbox_handle_upload(const struct user_t user, const struct socket_t client) {
    const struct dripbox_upload_header_t *upload_header = (void *) tls_dripbox_buffer;
    const size_t diff = sizeof *upload_header - sizeof(struct dripbox_generic_header_t);

    int got = recv(client.sock_fd, tls_dripbox_buffer + sizeof(struct dripbox_generic_header_t), diff, 0);
    if (got < 0) {
        log(LOG_INFO, "%s\n", strerror(errno));
        return;
    }

    got = recv(client.sock_fd, tls_dripbox_buffer + sizeof *upload_header, upload_header->file_name_length, 0);
    if (got < 0) {
        log(LOG_INFO, "%s\n", strerror(errno));
        return;
    }

    const struct string_view_t file_name = sv_substr(
        sv_from_cstr(tls_dripbox_buffer),
        sizeof *upload_header, upload_header->file_name_length
    );

    const struct string_view_t username = user.username;
    const size_t len = g_userdata_dir.length + username.length + strlen("/") + file_name.length + 1;

    got = snprintf(tls_dripbox_path_buffer,
                   len,
                   "./userdata/%*s/%*s",
                   (int) username.length, username.data,
                   (int) file_name.length, file_name.data);
    tls_dripbox_path_buffer[len] = 0;

    if (got < 0) {
        log(LOG_ERROR, "%s", strerror(errno));
        return;
    }

    if (sockdump_to_file(&client, tls_dripbox_path_buffer, upload_header->payload_length) != 0) {
        log(LOG_INFO, "%s\n", strerror(errno));
    }
}

void dripbox_handle_download(const struct user_t user, const struct socket_t client) {
    const struct dripbox_download_header_t *download_header = (void *) tls_dripbox_buffer;
    const size_t diff = sizeof *download_header - sizeof(struct dripbox_generic_header_t);

    int got = recv(client.sock_fd, tls_dripbox_buffer + sizeof(struct dripbox_generic_header_t), diff, 0);
    if (got < 0) {
        log(LOG_INFO, "%s\n", strerror(errno));
        return;
    }

    got = recv(client.sock_fd, tls_dripbox_buffer + sizeof *download_header, download_header->file_name_length, 0);
    if (got < 0) {
        log(LOG_INFO, "%s\n", strerror(errno));
        return;
    }

    const struct string_view_t file_name = sv_substr(
        sv_from_cstr(tls_dripbox_buffer),
        sizeof *download_header, download_header->file_name_length
    );

    const struct string_view_t username = user.username;
    const size_t len = strlen("./userdata/") + username.length + strlen("/") + file_name.length + 1;

    char path_buffer[PATH_MAX] = {};
    got = snprintf(path_buffer,
                   len,
                   "./userdata/%*s/%*s",
                   (int) username.length, username.data,
                   (int) file_name.length, file_name.data);
    path_buffer[len] = 0;

    if (got < 0) {
        log(LOG_ERROR, "%s\n", strerror(errno));
        return;
    }
    struct stat st = {};
    if (stat(path_buffer, &st) < 0) {
        const struct string_view_t sv_error = sv_from_cstr(strerror(errno));
        const struct dripbox_error_header_t error_header = {
            .version = 1,
            .type = MSG_ERROR,
            .error_length = sv_error.length,
        };
        if (send(client.sock_fd, &error_header, sizeof error_header, 0) < 0) {
            log(LOG_ERROR, "%s\n", strerror(errno));
            return;
        }
        if (send(client.sock_fd, sv_error.data, sv_error.length, 0) < 0) {
            log(LOG_ERROR, "%s\n", strerror(errno));
            return;
        }
        return;
    }

    scope(const int fd = open(path_buffer, O_RDONLY), close(fd)) {
        if (fd < 0) {
            log(LOG_INFO, "%s\n", strerror(errno));
            exit_scope;
        }

        const struct dripbox_upload_header_t upload_header = {
            .version = 1,
            .type = MSG_UPLOAD,
            .file_name_length = file_name.length,
            .payload_length = st.st_size,
        };

        if (send(client.sock_fd, &upload_header, sizeof upload_header, 0) < 0) {
            log(LOG_INFO, "%s\n", strerror(errno));
            exit_scope;
        }

        if (sendfile(client.sock_fd, fd, NULL, st.st_size) < 0) {
            log(LOG_INFO, "%s\n", strerror(errno));
        }
    }
}

static void handle_massage(void **users_hash_table,
                           const struct user_t user) {
    const struct socket_t client = user.socket;
    const struct dripbox_generic_header_t *header = (void *) tls_dripbox_buffer;

    const ssize_t got = recv(client.sock_fd, tls_dripbox_buffer, sizeof *header, 0);
    if (got == 0) { return; }
    if (got < 0) {
        log(LOG_INFO, "%s\n", strerror(errno));
        return;
    }

    tls_dripbox_buffer[got] = 0;
    const char *msg_type = msg_type_cstr(header->type);
    switch (header->type) {
    case MSG_NOOP: { return; }
    case MSG_LOGIN: {
        dripbox_handle_login(users_hash_table, client);
        break;
    }
    case MSG_UPLOAD: {
        dripbox_handle_upload(user, client);
        break;
    }
    case MSG_DOWNLOAD: {
        dripbox_handle_download(user, client);
        break;
    }
    default:
        log(LOG_INFO, "Invalid Message Type\n");
        break;
    }

    log(LOG_INFO, "Message Received\nVersion: %d\nType: %s\n", header->version, msg_type);
}

void *incoming_connections_worker(void *arg) {
    const struct incoming_connection_thread_context_t *context = arg;
    const struct tcp_listener_t *listener = context->listener;
    var addr = ipv4_endpoint_empty();

    socket_set_non_blocking(listener->sock_fd);
    bool quit = false;
    while (!quit) {
        struct socket_t client = {};
        while (tcp_server_incoming_next(listener, &client, &addr)) {
            handle_massage((void **) context->hash_table, (struct user_t){.socket = client});
        }
    }
    return NULL;
}

void *client_connections_worker(void *arg) {
    const struct incoming_connection_thread_context_t *context = arg;
    const struct tcp_listener_t *listener = context->listener;

    socket_set_non_blocking(listener->sock_fd);
    bool quit = false;
    while (!quit) {
        scope(pthread_mutex_lock(&g_users_mutex), pthread_mutex_unlock(&g_users_mutex)) {
            var users = *context->hash_table;
            for (int i = 0; i < hash_table_capacity(users); i++) {
                if (i >= hash_table_length(users)) { break; }
                const var entry = hash_set_entry(users, i);
                if (HASH_ENTRY_IS_NULL(entry)) { continue; }
                const var kvp = *(typeof(users)) entry->value;

                const struct user_t user = kvp.value;
                const struct socket_t client = user.socket;

                if (!socket_pending(&client, 0)) {
                    continue;
                }

                handle_massage((void **) &users, user);
            }
        }
    }
    return NULL;
}
