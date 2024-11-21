#include <stdio.h>
#include <dirent.h>
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

static bool dripbox_server_quit = false;

struct user_t {
    struct string_view username;
    struct socket socket;
};

typedef hash_table(char *, struct user_t) UsersHashTable;

struct incoming_connection_thread_context_t {
    struct tcp_listener *listener;
    UsersHashTable *hash_table;
};

struct payload_t {
    struct dripbox_login_header header;
    uint8_t *data;
};

const struct z_string g_userdata_dir = (struct z_string){
    .data = "userdata/",
    .length = sizeof "userdata/" - 1,
};

static void dripbox_server_handle_client_login(UsersHashTable *hash_table, struct socket *client);

static void dripbox_server_handle_client_upload(const UsersHashTable *hash_table, struct user_t *user);

static void dripbox_server_handle_client_download(struct user_t *user);

static void dripbox_server_handle_client_delete(const UsersHashTable *hash_table, struct user_t *user);

static void dripbox_server_handle_client_list(struct user_t *user);

static void dripbox_server_handle_client_massage(UsersHashTable *hash_table, struct user_t *user);

static void *dripbox_server_incoming_connections_worker(const void *arg);

static void *dripbox_server_network_worker(const void *arg);

static ssize_t dripbox_upload_client_file(
    struct user_t *user,
    struct string_view file_name,
    uint8_t checksum,
    struct z_string path
);

static void dripbox_delete_client_file(struct user_t *user, struct string_view file_name);

static int server_main() {
    signal(SIGPIPE, SIG_IGN);
    struct tcp_listener listener = tcp_listener_new(AF_INET);
    tcp_listener_bind(&listener, &ipv4_endpoint_new(ip, port));
    tcp_listener_listen(&listener, SOMAXCONN);
    if (listener.last_error != 0) {
        diagf(LOG_INFO, "%s\n", strerror(listener.last_error));
        return 1;
    }

    pthread_t incoming_connections_worker_id, nextwork_worker_id;
    var hash_table = hash_table_new(char*, struct user_t, string_hash, string_equals,
                                    &default_allocator);
    struct incoming_connection_thread_context_t ctx = {
        .listener = &listener,
        .hash_table = (UsersHashTable *) &hash_table,
    };

    pthread_create(&incoming_connections_worker_id, NULL, (void *) dripbox_server_incoming_connections_worker, &ctx);
    pthread_create(&nextwork_worker_id, NULL, (void *) dripbox_server_network_worker, &ctx);

    while (true) {
        sleep(1);
    }

    return 0;
}

struct monitor g_server_monitor  = MONITOR_INITIALIZER;

static void dripbox_server_handle_client_login(UsersHashTable *hash_table, struct socket *client) {
    const var login_header = socket_read_struct(client, struct dripbox_login_header, 0);
    const struct string_view username = sv_new(
        login_header.length,
        socket_read_array(client, char, login_header.length, 0)
    );

    const struct allocator *a = hash_set_allocator(*hash_table);
    const struct user_t _user = {
        .username = (struct string_view){
            .data = cstr_sv(username, a),
            .length = username.length,
        },
        .socket = *client,
    };

    const char *adress_cstr = ipv4_cstr(client->addr);
    hash_table_update(hash_table, adress_cstr, _user);

    const struct z_string dirpath = path_combine(g_userdata_dir, username);
    if (stat(g_userdata_dir.data, &(struct stat){}) < 0) {
        mkdir(g_userdata_dir.data, S_IRWXU | S_IRWXG | S_IRWXO);
    }
    if (stat(dirpath.data, &(struct stat){}) < 0) {
        mkdir(dirpath.data, S_IRWXU | S_IRWXG | S_IRWXO);
    }
}

ssize_t dripbox_upload_client_file(struct user_t *user,
                                      const struct string_view file_name,
                                      const uint8_t checksum,
                                      const struct z_string path) {
    struct socket *client = &user->socket;
    struct stat st;
    if (stat(path.data, &st) < 0) {
        dripbox_send_error(client, errno, path.data);
        return 0;
    }

    if (S_ISDIR(st.st_mode)) {
        dripbox_send_error(client, EISDIR, path.data);
        return 0;
    }

    scope(FILE* file = fopen(path.data, "rb"), file && fclose(file)) {
        if (file == NULL) {
            dripbox_send_error(client, errno, path.data);
            continue;
        }

        socket_write_struct(client, ((struct dripbox_msg_header){
            .version = 1,
            .type = MSG_UPLOAD,
        }), 0);

        socket_write_struct(client, ((struct dripbox_upload_header){
            .file_name_length = file_name.length,
            .payload_length = st.st_size,
        }), 0);

        socket_write(client, sv_deconstruct(file_name), 0);
        socket_write_struct(client, checksum, 0);
        socket_write_file(client, file, st.st_size);
    }
    return st.st_size;
}

void dripbox_delete_client_file(struct user_t *user, const struct string_view file_name) {
    struct socket *client = &user->socket;

    socket_write_struct(client, ((struct dripbox_msg_header) {
        .version = 1,
        .type = MSG_DELETE,
    }), 0);

    socket_write_struct(client, ((struct dripbox_delete_header) {
        .file_name_length = file_name.length,
    }), 0);

    socket_write(client, sv_deconstruct(file_name), 0);

    if (client->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(client->error));
    }
}

static void dripbox_server_handle_client_upload(const UsersHashTable *hash_table, struct user_t *user) {
    struct socket *client = &user->socket;
    const var upload_header = socket_read_struct(client, struct dripbox_upload_header, 0);
    const struct string_view file_name = sv_new(
        upload_header.file_name_length,
        socket_read_array(client, char, upload_header.file_name_length, 0)
    );
    const uint8_t client_checksum = socket_read_struct(client, uint8_t, 0);
    const struct string_view username = user->username;
    const struct z_string path = path_combine(g_userdata_dir, username, file_name);

    if (stat(path.data, &(struct stat){}) < 0) { goto upload_file; }
    if (dripbox_file_checksum(path.data) == client_checksum) {
        socket_redirect_to_file(client, "/dev/null", upload_header.payload_length);
        return;
    }
upload_file:
    socket_redirect_to_file(client, path.data, upload_header.payload_length);
    if (client->error != 0) {
        diagf(LOG_INFO, "%s\n", strerror(client->error));
        return;
    }

    const var ht = hash_table_header(*hash_table);
    for (int i = 0; i < ht->capacity; i++) {
        const var entry = hash_set_entry(*hash_table, i);
        if (HASH_ENTRY_IS_NULL(entry)) { continue; }

        const var kvp = (UsersHashTable) entry->value;
        if (!sv_equals(kvp->value.username, username)) { continue; }
        if (kvp->value.socket.sock_fd == user->socket.sock_fd) { continue; }
        if (kvp->value.socket.sock_fd == -1) { continue; }

        dripbox_upload_client_file(&kvp->value, file_name, client_checksum, path);
        diagf(LOG_INFO, "Uploaded "sv_fmt" to "sv_fmt" Size=%ld Ip=%s Checksum: 0X%X\n",
              (int)sv_deconstruct(file_name),
              (int)sv_deconstruct(kvp->value.username),
              upload_header.payload_length,
              kvp->key,
              client_checksum
        );
    }
}

void dripbox_server_handle_client_download(struct user_t *user) {
    struct socket *client = &user->socket;
    const var download_header = socket_read_struct(client, struct dripbox_download_header, 0);
    const var file_name = sv_new(
        download_header.file_name_length,
        socket_read_array(client, char, download_header.file_name_length, 0)
    );

    if (client->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(client->error));
        return;
    }

    const struct string_view username = user->username;
    const struct z_string path = path_combine(g_userdata_dir, username, file_name);

    struct stat st = {};
    if (stat(path.data, &st) < 0) {
        dripbox_send_error(client, errno, path.data);
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        dripbox_send_error(client, EISDIR, path.data);
        return;
    }

    diagf(LOG_INFO, "Uploading "sv_fmt" to "sv_fmt" Size=%ld",
        (int)sv_deconstruct(file_name),
        (int)sv_deconstruct(username),
        st.st_size
    );

    scope(FILE* file = fopen(path.data, "rb"), fclose(file)) {
        if (file == NULL) {
            dripbox_send_error(client, errno, path.data);
            break;
        }

        socket_write_struct(client, ((struct dripbox_msg_header) {
            .version = 1,
            .type = MSG_BYTES,
        }), 0);

        socket_write_struct(client, ((struct dripbox_bytes_header) {
            .length = st.st_size,
        }), 0);

        socket_write_file(client, file, st.st_size);
        if (client->error != 0) {
            diagf(LOG_ERROR, "%s\n", strerror(client->error));
        }
    }
}

void dripbox_server_handle_client_delete(const UsersHashTable *hash_table, struct user_t *user) {
    struct socket *client = &user->socket;
    const var delete_header = socket_read_struct(client, struct dripbox_delete_header, 0);
    const struct string_view file_name = sv_new(
        delete_header.file_name_length,
        socket_read_array(client, char, delete_header.file_name_length, 0)
    );
    const struct string_view username = user->username;
    const struct z_string path = path_combine(g_userdata_dir, username, file_name);

    if (client->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(client->error));
        return;
    }

    if (stat(path.data, &(struct stat){}) < 0) {
        dripbox_send_error(client, errno, path.data);
        return;
    }

    if (unlink(path.data) < 0) {
        diagf(LOG_ERROR, "%s\n", strerror(errno));
    } else {
        diagf(LOG_INFO, "Deleted "sv_fmt"\n", (int)sv_deconstruct(file_name));
    }

    for (int i = 0; i < hash_table_capacity(*hash_table); i++) {
        const var entry = hash_set_entry(*hash_table, i);
        if (HASH_ENTRY_IS_NULL(entry)) { continue; }

        const var kvp = (UsersHashTable) entry->value;
        if (!sv_equals(kvp->value.username, user->username)) { continue; }
        if (kvp->value.socket.sock_fd == user->socket.sock_fd) { continue; }
        if (kvp->value.socket.sock_fd == -1) { continue; }

        dripbox_delete_client_file(&kvp->value, file_name);

        diagf(LOG_INFO, "Deleting "sv_fmt" from "sv_fmt" Ip=%s\n",
              (int)sv_deconstruct(file_name),
              (int)sv_deconstruct(kvp->value.username),
              kvp->key
        );
    }
}

static void dripbox_server_handle_client_massage(UsersHashTable *hash_table, struct user_t *user) {
    struct socket *client = &user->socket;
    const var msg_header = socket_read_struct(client, struct dripbox_msg_header, 0);

    if (zero_initialized(msg_header)) {
        return;
    }

    if (!dripbox_expect_version(client, msg_header.version, 1)) {
        return;
    }

    const char *msg_type = msg_type_cstr(msg_header.type);
    diagf(LOG_INFO, "Received Message %s\n", msg_type);
    switch (msg_header.type) {
    case MSG_NOOP: break;
    case MSG_LOGIN: {
        dripbox_server_handle_client_login(hash_table, client);
        break;
    }
    case MSG_UPLOAD: {
        dripbox_server_handle_client_upload(hash_table, user);
        break;
    }
    case MSG_DOWNLOAD: {
        dripbox_server_handle_client_download(user);
        break;
    }
    case MSG_DELETE: {
        dripbox_server_handle_client_delete(hash_table, user);
        break;
    }
    case MSG_LIST: {
        dripbox_server_handle_client_list(user);
        break;
    }
    default:
        diagf(LOG_INFO, "Type: 0X%X\n", msg_header.type);
        break;
    }
    if (client->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(client->error));
        client->error = 0;
    }
}

void *dripbox_server_incoming_connections_worker(const void *arg) {
    const struct incoming_connection_thread_context_t *context = arg;
    const struct tcp_listener *listener = context->listener;
    var addr = ipv4_endpoint_empty();

    socket_set_non_blocking(listener->sock_fd);
    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (!dripbox_server_quit) {
        struct socket client = {};
        while (tcp_server_incoming_next(listener, &client, &addr)) {
            using_monitor(&g_server_monitor) {
                dripbox_server_handle_client_massage(context->hash_table, &(struct user_t){ .socket = client });
            }
        }
    }
    return NULL;
}

void *dripbox_server_network_worker(const void *arg) {
    const struct incoming_connection_thread_context_t *context = arg;
    const struct tcp_listener *listener = context->listener;

    socket_set_non_blocking(listener->sock_fd);
    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (!dripbox_server_quit) {
        using_monitor(&g_server_monitor) {
            UsersHashTable *users = context->hash_table;
            for (int i = 0; i < hash_table_capacity(*users); i++) {
                const var entry = hash_set_entry(*users, i);
                if (HASH_ENTRY_IS_NULL(entry)) { continue; }
                const var kvp = (UsersHashTable) entry->value;
                struct user_t *user = &kvp->value;
                if (user->username.length <= 0) { continue; }

                struct socket client = user->socket;
                if (client.sock_fd == -1) { continue; }

                if (!socket_pending(&client, 0)) { continue; }
                dripbox_server_handle_client_massage(users, user);
            }
        }
    }
    return NULL;
}

void dripbox_server_handle_client_list(struct user_t *user) {
    struct dirent **server_files;
    const struct z_string client_dir = path_combine(g_userdata_dir, user->username);
    const int files_count = scandir(client_dir.data, &server_files, dripbox_dirent_is_file, alphasort);

    struct dripbox_file_stat dripbox_sts[files_count];
    memset(dripbox_sts, 0, sizeof dripbox_sts);

    for (int i = 0; i < files_count; i++) {
        const struct dirent *entry = server_files[i];
        struct dripbox_file_stat *dripbox_st = &dripbox_sts[i];
        const struct z_string file_path = path_combine(client_dir, entry->d_name);

        struct stat st = {};
        if (stat(file_path.data, &st) >= 0) {
            dripbox_st->ctime = st.st_ctime;
            dripbox_st->atime = st.st_atime;
            dripbox_st->mtime = st.st_mtime;
            dripbox_st->checksum = dripbox_file_checksum(file_path.data);
        }
        else {
            diagf(LOG_INFO, "%s %s\n", strerror(errno), file_path.data);
        }
        strncpy(dripbox_st->name, entry->d_name, sizeof entry->d_name - 1);
    }

    struct socket *client = &user->socket;

    socket_write_struct(client, ((struct dripbox_msg_header) {
        .version = 1,
        .type = MSG_LIST,
    }), 0);

    socket_write_struct(client, ((struct dripbox_list_header) {
        .file_list_length = files_count,
    }), 0);

    socket_write(client, size_and_address(dripbox_sts), 0);
}

static void dripbox_server_shutdown(void) {
    dripbox_server_quit = true;
}
