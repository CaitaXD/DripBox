#include <Allocator.h>
#include <Monitor.h>
#include <stdio.h>
#include <common.h>
#include <Network.h>
#include <string.h>
#include <dripbox_common.h>
#include <inotify_common.h>
#include <dynamic_array.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include  "string_view.h"
#include "array.h"

typedef tuple(struct socket, uint32_t) socket_ip_tuple_t;
struct dripbox_client {
    struct socket leader_socket;
    uint32_t leader_in_addr;
    dynamic_array(socket_ip_tuple_t) replica_sockets;
    struct dripbox_msg_header discovery_probe;
    struct monitor m_replicas;
    enum election_state election_state;
} dripbox_client;

struct string_view dripbox_username = {};
bool dripbox_client_quit = false;

typedef dynamic_array(struct z_string) ZStringDynamicArray;

ZStringDynamicArray g_inotify_supress_list = NULL;

const struct string_view g_sync_dir_path ={
    .data = "sync_dir/",
    .length = sizeof "sync_dir/" - 1,
};

static const struct string_view g_cmd_upload = {
    .data = "upload",
    .length = sizeof "upload" - 1,
};

static const struct string_view g_cmd_download = {
    .data = "download",
    .length = sizeof "download" - 1,
};

static const struct string_view g_cmd_list_client = {
    .data = "list_client",
    .length = sizeof "list_client" - 1,
};

static const struct string_view g_cmd_list_server = {
    .data = "list_server",
    .length = sizeof "list_server" - 1,
};

static const struct string_view cmd_exit = {
    .data = "exit",
    .length = sizeof "exit" - 1,
};

static const struct string_view cmd_shell = {
    .data = "shell",
    .length = sizeof "shell" - 1,
};

static bool fd_pending_read(const int fd) {
    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
    };
    const int event_count = poll(&pfd, 1, 0);
    if (event_count > 0) {
        return pfd.revents & POLLIN;
    }
    return false;
}

struct monitor g_client_monitor  = MONITOR_INITIALIZER;

static int dripbox_client_login(struct socket *s, struct string_view username);

static int dripbox_client_upload(struct socket *s, char *file_path);

static int dripbox_client_download(struct socket *s, char *file_path);

static void *dripbox_client_network_worker(const void *args);

static void dripbox_client_command_dispatch(struct socket *s, struct string_view cmd);

static void dripbox_client_handle_server_message(struct socket *s);

static int dripbox_client_list_server(struct socket *s, bool update_client_list);

static void* client_discover_servers_worker(void *arg);

static void dripbox_ensure_sock(void);

void dripbox_client_list(struct string_view sync_dir_path);

void dripbox_cleint_inotify_dispatch(struct socket *s, struct inotify_event_t inotify_event);

void *inotify_watcher_worker(const void *args);

int client_main() {
    pthread_t inotify_watcher_worker_id, network_worker_id, client_discover_servers_worker_id;
    struct stat dir_stat = {};
    if (stat(g_sync_dir_path.data, &dir_stat) < 0) {
        mkdir(g_sync_dir_path.data, S_IRWXU | S_IRWXG | S_IRWXO);
    }

    var server_endpoint = ipv4_endpoint(ntohl(ip), port);
    var s = socket_new();
    socket_open(&s, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!tcp_client_connect(&s, &server_endpoint)) {
        diagf(LOG_ERROR, "%s\n", strerror(s.error.code));
        return -1;
    }

    dripbox_client_login(&s, dripbox_username);
    dripbox_client_list_server(&s, true);

    if (s.error.code != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(s.error.code));
        return -1;
    }

    dripbox_client = (struct dripbox_client) {
        .leader_socket = s,
        .leader_in_addr = socket_address_get_in_addr(&server_endpoint),
        .replica_sockets = (void*)dynamic_array_new(socket_ip_tuple_t, &mallocator),
        .m_replicas = MONITOR_INITIALIZER,
        .discovery_probe = { .version = 1, .type = DRIP_MSG_LOGIN },
    };

    pthread_create(&inotify_watcher_worker_id, NULL, (void *) inotify_watcher_worker, &dripbox_client.leader_socket);
    pthread_create(&network_worker_id, NULL, (void *) dripbox_client_network_worker, &dripbox_client.leader_socket);
    pthread_create(&client_discover_servers_worker_id, NULL, (void *) client_discover_servers_worker, &dripbox_client);

    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (!dripbox_client_quit) {
        dripbox_ensure_sock();

        if (!fd_pending_read(STDIN_FILENO)) continue;

        const struct string_view buffer = sv_stack(1024);
        const struct string_view cmd = sv_cstr(fgets(buffer.data, buffer.length, stdin));
        if (cmd.length == 0) continue;

        dripbox_client_command_dispatch(&dripbox_client.leader_socket, cmd);
    }

    pthread_join(network_worker_id, NULL);
    //pthread_join(inotify_watcher_worker_id, NULL);

    return 0;
}

static void dripbox_client_command_dispatch(struct socket *s, const struct string_view cmd) {
    // Upload
    if (strncmp(cmd.data, g_cmd_upload.data, g_cmd_upload.length) == 0) {
        struct string_view file_path = sv_token(cmd, sv_cstr(" "))[1];

        if (file_path.data[file_path.length - 1] == '\n') {
            file_path.data[file_path.length - 1] = 0;
            file_path.length -= 1;
        }

        var it = sv_split(file_path, "/");
        const struct string_view *file_name = iterator_last(&it);

        const struct z_string file_sync_path = path_combine(g_sync_dir_path, *file_name);
        const struct z_string swap_file = zconcat(file_sync_path, ".swp");
        dynamic_array_push(&g_inotify_supress_list, swap_file);
        if(dripbox_transaction_copy_file(*(struct z_string*)&file_path, file_sync_path) != TRNASACTION_COMMITTED) {
            return;
        }
        dripbox_client_upload(s, file_path.data);
    }
    // Download
    else if (strncmp(cmd.data, g_cmd_download.data, g_cmd_download.length) == 0) {
        const struct string_view file_path = sv_token(cmd, sv_cstr(" "))[1];
        if (file_path.data[file_path.length - 1] == '\n') {
            file_path.data[file_path.length - 1] = 0;
        }
        dripbox_client_download(s, file_path.data);
    }
    // List Client
    else if (strncmp(cmd.data, g_cmd_list_client.data, g_cmd_list_client.length) == 0) {
        dripbox_client_list(g_sync_dir_path);
    }
    // List Server
    else if (strncmp(cmd.data, g_cmd_list_server.data, g_cmd_list_server.length) == 0) {
        dripbox_client_list_server(s, false);
    }
    // Shell
    else if (strncmp(cmd.data, cmd_shell.data, cmd_shell.length) == 0) {
        const struct string_view shell = sv_token(cmd, sv_cstr(" "))[1];
        if (shell.data[shell.length - 1] == '\n') {
            shell.data[shell.length - 1] = 0;
        }
        system(shell.data);
    }
    // Exit
    else if (strncmp(cmd.data, cmd_exit.data, cmd_exit.length) == 0) {
        dripbox_client_quit = true;
    }
    else {
        diagf(LOG_ERROR, "Unknown command: %s\n", cmd.data);
    }
}

void *dripbox_client_network_worker(const void *args) {
    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (!dripbox_client_quit) {
        struct socket *s = &dripbox_client.leader_socket;
        if (s->error.code == EAGAIN) s->error.code = 0;
        if (s->error.code != 0 || s->sock_fd == -1) continue;
        dripbox_ensure_sock();
        if (socket_pending_read(s, 0)) {
            using_monitor(&g_client_monitor) {
                dripbox_client_handle_server_message(s);
            }
        }
    }
    socket_close(&dripbox_client.leader_socket);
    return NULL;
}

int dripbox_client_login(struct socket *s, const struct string_view username) {
    diagf(LOG_INFO, "Logging in to server\n");
    if (s->error.code != 0) { return -1; }

    socket_write_struct(s, ((struct dripbox_msg_header){
        .version = 1,
        .type = DRIP_MSG_LOGIN,
    }), 0);

    socket_write_struct(s, ((struct dripbox_login_header){
        .length = username.length,
    }), 0);

    socket_write(s, sv_deconstruct(username), 0);

    if (s->error.code != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(s->error.code));
        return -1;
    }

    diagf(LOG_INFO, "Logged in\n");
    return 0;
}

int dripbox_client_upload(struct socket *s, char *file_path) {
    if (s->error.code != 0) { return -1; }

    var it = sv_split(file_path, sv_cstr("/"));
    const struct string_view *file_name = iterator_last(&it);

    struct stat st = {};
    if (stat(file_path, &st) < 0) {
        diagf(LOG_ERROR, "%s %s\n", strerror(errno), file_path);
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        dripbox_send_error(s, EISDIR,SV( file_path));
        return -1;
    }

    const uint8_t checksum = dripbox_file_checksum(file_path);

    diagf(LOG_INFO, "Uploaded "sv_fmt" Size=%ld Checksum: 0X%X\n",
        (int)sv_deconstruct(*file_name),
        st.st_size,
        checksum
    );

    socket_write_struct(s, ((struct dripbox_msg_header) {
        .version = 1,
        .type = DRIP_MSG_UPLOAD,
    }), 0);

    socket_write_struct(s, ((struct dripbox_upload_header) {
        .file_name_length = file_name->length,
        .payload_length = st.st_size,
    }), 0);

    socket_write(s, sv_deconstruct(*file_name), 0);
    socket_write_struct(s, checksum, 0);

    scope(FILE *file = fopen(file_path, "rb"), file && fclose(file)) {
        if (file == NULL) {
            diagf(LOG_ERROR, "%s\n", strerror(errno));
            break;
        }
        socket_write_file(s, file, st.st_size);
    }

    if (s->error.code != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(s->error.code));
        return -1;
    }
    return 0;
}

int dripbox_delete_from_server(struct socket *s, char *file_path) {
    if (s->error.code != 0) { return -1; }

    var it = sv_split(file_path, sv_cstr("/"));
    const struct string_view *file_name = iterator_last(&it);

    socket_write_struct(s, ((struct dripbox_msg_header) {
        .version = 1,
        .type = DRIP_MSG_DELETE,
    }), 0);

    socket_write_struct(s, ((struct dripbox_delete_header) {
        .file_name_length = file_name->length,
    }), 0);

    socket_write(s, sv_deconstruct(*file_name), 0);

    if (s->error.code != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(s->error.code));
        return -1;
    }

    diagf(LOG_INFO, "Sent "sv_fmt" for deletion\n", (int)sv_deconstruct(*file_name));
    return 0;
}

int dripbox_client_download(struct socket *s, char *file_path) {
    if (s->error.code != 0) { return -1; }

    var it = sv_split(file_path, sv_cstr("/"));
    const struct string_view *file_name = iterator_last(&it);

    socket_write_struct(s, ((struct dripbox_msg_header) {
        .version = 1,
        .type = DRIP_MSG_DOWNLOAD,
    }), 0);

    socket_write_struct(s, ((struct dripbox_download_header) {
        .file_name_length = file_name->length,
    }), 0);

    socket_write(s, sv_deconstruct(*file_name), 0);

    const var msg_header = socket_read_struct(s, struct dripbox_msg_header, 0);
    if (!dripbox_expect_version(s, msg_header.version, 1)) return -1;
    if (!dripbox_expect_msg(s, msg_header.type, DRIP_MSG_BYTES)) return -1;

    const var bytes_header = socket_read_struct(s, struct dripbox_bytes_header, 0);
    diagf(LOG_INFO, "Downloading "sv_fmt" Size=%ld\n",
        (int)sv_deconstruct(*file_name),
        bytes_header.length
    );

    return socket_redirect_to_file(s, file_path, bytes_header.length);
}

void dripbox_client_list(const struct string_view sync_dir_path) {
    struct dirent **namelist;
    int n = scandir(sync_dir_path.data, &namelist, dripbox_dirent_is_file, alphasort);

    struct stat statbuf;
    printf("%d", n);
    printf("\n\n==== Local Client\'s Files: ====\n\n");
    while (n--) {
        const struct string_view file_name = (struct string_view){
            .data = namelist[n]->d_name,
            .length = strlen(namelist[n]->d_name),
        };

        const struct z_string path = path_combine(sync_dir_path, file_name);

        if (stat(path.data, &statbuf) >= 0) {
            const struct tm *tm_ctime = localtime(&statbuf.st_ctime);
            const struct tm *tm_atime = localtime(&statbuf.st_atime);
            const struct tm *tm_mtime = localtime(&statbuf.st_mtime);

            printf(dripbox_file_stat_fmt"\n",
                namelist[n]->d_name,
                tm_long_data_deconstruct(tm_ctime),
                tm_long_data_deconstruct(tm_atime),
                tm_long_data_deconstruct(tm_mtime),
                dripbox_file_checksum(path.data)
            );
        }
    }
}

void dripbox_cleint_inotify_dispatch(struct socket *s, struct inotify_event_t inotify_event) {
    int i = 0;
    struct inotify_event *event = NULL;
    struct inotify_event *last_event = NULL;
    while (i < inotify_event.buffer_len) {
        event = (struct inotify_event *) &inotify_event.event_buffer[i];
        i += EVENT_SIZE + event->len;
        last_event = event;
        event = (struct inotify_event *) &inotify_event.event_buffer[i];
        i += EVENT_SIZE + event->len;
        if (i >= inotify_event.buffer_len) { break; }
    }
    event = last_event;

    // Ignore dot files
    if (sv_starts_with(event->name, ".")) {
        return;
    }
    // Ignore backup files
    if (sv_ends_with(event->name, "~")) {
        return;
    }

    assert(event && "Nougthy");
    if (event->mask >= IN_ISDIR) { event->mask %= IN_ISDIR; }

    const struct z_string fullpath = path_combine(g_sync_dir_path, (char*)event->name);

    const size_t index = dynamic_array_index_of(g_inotify_supress_list, fullpath, sv_comparer_equals);
    if (index != NOT_FOUND) {
        if((event->mask & (IN_DELETE | IN_MOVED_FROM)) != 0) {
            dynamic_array_remove_at(&g_inotify_supress_list, index);
        }
        return;
    }

    switch (event->mask) {
    case IN_MOVED_TO:
        diagf(LOG_INFO, "Moved %s in\n", event->name);
        dripbox_client_upload(s, fullpath.data);
        break;
    case IN_MOVED_FROM:
        diagf(LOG_INFO, "%s Moved out\n", event->name);
        dripbox_delete_from_server(s, event->name);
        break;
    case IN_DELETE:
        diagf(LOG_INFO, "Deleted %s\n", event->name);
        dripbox_delete_from_server(s, event->name);
        break;
    case IN_DELETE_SELF:
        diagf(LOG_ERROR, "[NOT IMPLEMENTED] IN_DELETE_SELF\n");
        break;
    case IN_CLOSE_WRITE:
        diagf(LOG_INFO, "Closed %s\n", event->name);
        dripbox_client_upload(s, fullpath.data);
        break;
    default:
        printf("OTHER\n");
    }
}

void *inotify_watcher_worker(const void *args) {
    struct socket *s = (struct socket *) args;

    const struct inotify_watcher_t watcher = init_inotify(-1, g_sync_dir_path.data);
    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (!dripbox_client_quit) {
        const struct inotify_event_t inotify_event = read_event(watcher);
        if (inotify_event.error == EAGAIN) continue;

        dripbox_cleint_inotify_dispatch(s, inotify_event);
    }

    const int ret_value = inotify_rm_watch(watcher.inotify_fd, watcher.watcher_fd);
    if (ret_value) {
        diagf(LOG_ERROR, "Inotify watch remove: %s\n", strerror(errno));
        return (void *) -1;
    }

    return 0;
}

void dripbox_handle_server_delete(struct socket *s) {
    const var delete_header = socket_read_struct(s, struct dripbox_delete_header, 0);
    const struct string_view file_name = sv_new(
        delete_header.file_name_length,
        socket_read_array(s, char, delete_header.file_name_length, 0)
    );
    if (s->error.code != 0) { return; }

    const struct z_string path = path_combine(g_sync_dir_path, file_name);
    if (unlink(path.data) < 0) {
        diagf(LOG_ERROR, "%s\n", strerror(errno));
        return;
    }
    diagf(LOG_INFO, "Deleted "sv_fmt"\n", (int)sv_deconstruct(file_name));
}

void dripbox_handle_server_upload(struct socket *s) {
    const var upload_header = socket_read_struct(s, struct dripbox_upload_header, 0);

    const struct string_view file_name = sv_new(
        upload_header.file_name_length,
        socket_read_array(s, char, upload_header.file_name_length, 0)
    );

    const uint8_t server_checksum = socket_read_struct(s, uint8_t, 0);
    const var path = path_combine(g_sync_dir_path, file_name);
    if (stat(path.data, &(struct stat){}) < 0) { goto send_file; }

    const uint8_t checksum = dripbox_file_checksum(path.data);
    if (checksum == server_checksum) {
        socket_redirect_to_file(s, "/dev/null", upload_header.payload_length);
        return;
    }
send_file:
    if (socket_redirect_to_file(s, path.data, upload_header.payload_length) != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(errno));
    }
}

void dripbox_client_handle_server_message(struct socket *s) {
    const var msg_header = socket_read_struct(s, struct dripbox_msg_header, 0);
    if (!dripbox_expect_version(s, msg_header.version, 1)) return;

    diagf(LOG_INFO, "Message { Version: 0X%X Type:'%s' }\n", msg_header.version, msg_type_cstr(msg_header.type));
    switch (msg_header.type) {
    case DRIP_MSG_NOOP:
        break;
    case DRIP_MSG_UPLOAD: {
        dripbox_handle_server_upload(s);
        break;
    }
    case DRIP_MSG_DELETE: {
        dripbox_handle_server_delete(s);
        break;
    }
    case DRIP_MSG_COORDINATOR: {
        const var coordinator_header = socket_read_struct(s, struct dripbox_coordinator_header, 0);
        for (int i = 0; i < dynamic_array_length(dripbox_client.replica_sockets); i++) {
            const socket_ip_tuple_t *socket_ip_tuple = &dripbox_client.replica_sockets[i];
            if (socket_ip_tuple->item2 == coordinator_header.coordinator_inaddr) {
                dripbox_client.leader_socket = socket_ip_tuple->item1;
                dripbox_client.leader_in_addr = socket_ip_tuple->item2;
                dripbox_client.election_state = ELECTION_STATE_NONE;
                diagf(LOG_INFO, "Found coordinator\n");
                break;
            }
        }
    } break;
    case DRIP_MSG_ERROR: {
        const struct string_view sv_error = dripbox_read_error(s);
        diagf(LOG_ERROR, "Dripbox error: "sv_fmt"\n", (int)sv_deconstruct(sv_error));
        break;
    }
    default:
        break;
    }
    if (s->error.code != 0) { diagf(LOG_ERROR, "%s\n", strerror(s->error.code)); }
}

struct file_name_checksum_tuple {
    struct string_view name;
    uint8_t checksum;
};

bool file_name_checksum_tuple_name_equals(const void *a, const void *b) {
    struct file_name_checksum_tuple *a_ = (struct file_name_checksum_tuple *) a;
    struct file_name_checksum_tuple *b_ = (struct file_name_checksum_tuple *) b;
    return sv_equals(a_->name, b_->name);
}

bool file_name_checksum_tuple_equals(const void *a, const void *b) {
    struct file_name_checksum_tuple *a_ = (struct file_name_checksum_tuple *) a;
    struct file_name_checksum_tuple *b_ = (struct file_name_checksum_tuple *) b;
    return sv_equals(a_->name, b_->name) && a_->checksum == b_->checksum;
}


int dripbox_client_list_server(struct socket *s, const bool update_client_list) {
    diag(LOG_INFO, "Listing server files");
    if (s->error.code != 0) { return -1; }

    socket_write_struct(s, ((struct dripbox_msg_header) {
        .version = 1,
        .type = DRIP_MSG_LIST,
    }), 0);

    const var msg_header = socket_read_struct(s, struct dripbox_msg_header, 0);
    if (!dripbox_expect_version(s, msg_header.version, 1)) return -1;
    if (!dripbox_expect_msg(s, msg_header.type, DRIP_MSG_LIST)) return -1;

    const var upload_header = socket_read_struct(s, struct dripbox_list_header, 0);
    const int server_stats_count = upload_header.file_list_length;
    struct dripbox_file_stat server_stats[server_stats_count];
    socket_read_exactly(s, size_and_address(server_stats), 0);

    if(server_stats_count > 0) {
        printf("\n\n==== Client\'s server files ====\n\n");
    }
    for (int i = 0; i < server_stats_count; i++) {
        const struct dripbox_file_stat server_st = server_stats[i];
        time_t ctime = server_st.ctime;
        time_t atime = server_st.atime;
        time_t mtime = server_st.mtime;
        const struct tm *tm_ctime = localtime(&ctime);
        const struct tm *tm_atime = localtime(&atime);
        const struct tm *tm_mtime = localtime(&mtime);
        printf(dripbox_file_stat_fmt,
            server_st.name,
            tm_long_data_deconstruct(tm_ctime),
            tm_long_data_deconstruct(tm_atime),
            tm_long_data_deconstruct(tm_mtime),
            server_st.checksum
        );
        if (i >= server_stats_count - 1) {
            printf("\n");
        }
        printf("\n");
    }
    if (update_client_list && server_stats_count > 0) {
        struct dirent **client_entries;
        const int client_entries_count = scandir(g_sync_dir_path.data, &client_entries, dripbox_dirent_is_file, alphasort);
        var client_set = array_stack(struct file_name_checksum_tuple, client_entries_count);
        for (int i = 0; i < client_entries_count; i++) {
            struct dirent *entry = client_entries[i];
            const struct z_string path = path_combine(g_sync_dir_path, entry->d_name);
            client_set[i] = (struct file_name_checksum_tuple) {
                .name = sv_cstr(entry->d_name),
                .checksum = dripbox_file_checksum(path.data),
            };
        }
        var server_set = array_stack(struct file_name_checksum_tuple, server_stats_count);
        for (int i = 0; i < server_stats_count; i++) {
            struct dripbox_file_stat *stat = &server_stats[i];
            server_set[i] = (struct file_name_checksum_tuple) {
                .name = sv_cstr(stat->name),
                .checksum = stat->checksum,
            };
        }
        using_allocator_temp_arena {
            struct allocator *tempa = &allocator_temp_arena()->allocator;
            // Download = { file | file ∈ Server \ Client }
            var to_download = array_set_difference(server_set, client_set, file_name_checksum_tuple_equals, tempa);
            diagf(LOG_INFO, "Downloading %ld files\n", array_length(to_download));
            // Delete = { file | file ∈ (Client ↦ file_name) \ (Server ↦ file_name) }
            var to_delete = array_set_difference(client_set, server_set, file_name_checksum_tuple_name_equals, tempa);
            diagf(LOG_INFO, "Deleting %ld files\n", array_length(to_delete));

            for (int i = 0; i < array_length(to_delete); i++) {
                struct file_name_checksum_tuple item = to_delete[i];
                const struct z_string path = path_combine(g_sync_dir_path, item.name);
                if (unlink(path.data) < 0) {
                    diagf(LOG_ERROR, "%s %s\n", strerror(errno), item.name.data);
                    continue;
                }
                diagf(LOG_INFO, "Deleted "sv_fmt"\n", (int)sv_deconstruct(item.name));
            }
            for (int i = 0; i < array_length(to_download); i++) {
                struct file_name_checksum_tuple item = to_download[i];
                const struct z_string path = path_combine(g_sync_dir_path, item.name);
                if(dripbox_client_download(s, path.data) < 0) {
                    diagf(LOG_ERROR, "Failed to Download %s\n", path.data);
                }
            }
        }
    }
    return 0;
}

void* client_discover_servers_worker(void *arg) {
    struct dripbox_client *dripbox_client = arg;
    struct socket multicast_sock = socket_new();
    struct socket_address remote = ipv4_endpoint(0, 0);
    const struct socket_address multicast_addr = ipv4_endpoint(DRIPBOX_REPLICA_MULTICAST_GROUP, DRIPBOX_REPLICA_PORT);

    socket_open(&multicast_sock, AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    socket_reuse_address(&multicast_sock, true);
    socket_bind(&multicast_sock, &ipv4_endpoint(INADDR_ANY, DRIPBOX_REPLICA_PORT));
    socket_join_multicast_group(&multicast_sock, (struct ip_mreq) {
        .imr_multiaddr = htonl(DRIPBOX_REPLICA_MULTICAST_GROUP),
        .imr_interface = INADDR_ANY,
    });
    socket_option(&multicast_sock, IPPROTO_IP, IP_MULTICAST_LOOP, false);

    socket_write_struct_to(&multicast_sock, dripbox_client->discovery_probe, &multicast_addr, 0);
    while (!dripbox_client_quit) {
        typedef typeof(dripbox_server.discovery_probe) discovery_probe_t;
        const var discovery_response = socket_read_struct_from(&multicast_sock, discovery_probe_t, &remote, 0);
        if (socket_address_get_in_addr(&remote) == dripbox_client->leader_in_addr) continue;

        if(!dripbox_expect_version(&multicast_sock, discovery_response.item1.version, 1)) continue;
        if(!dripbox_expect_msg(&multicast_sock, discovery_response.item1.type, DRIP_MSG_ADD_REPLICA)) continue;

        using_monitor(&dripbox_client->m_replicas) {
            struct socket replica_socket = socket_new();
            socket_open(&replica_socket, AF_INET, SOCK_STREAM, IPPROTO_TCP);
            socket_reuse_address(&replica_socket, true);
            socket_adress_set_port(&remote, port);
            socket_connect(&replica_socket, &remote);
            dripbox_client_login(&replica_socket, dripbox_username);
            if (replica_socket.error.code != 0) {
                ediag("client_discover_servers_worker");
                continue;
            }
            diagf(LOG_INFO, "Connected to replica\n");
            dynamic_array_push(&dripbox_client->replica_sockets, ((socket_ip_tuple_t) {
                .item1 = replica_socket,
                .item2 = ((struct sockaddr_in*)remote.sa)->sin_addr.s_addr,
            }));
        }
    }
    return NULL;
}


void dripbox_ensure_sock(void) {
    struct socket *s = &dripbox_client.leader_socket;
    const bool badsock = socket_pending_event(s, POLLHUP | POLLNVAL | POLLERR, 0);
    if (badsock || s->sock_fd == -1  || s->error.code != 0) using_monitor(&g_client_monitor) {
        if (dripbox_client.election_state == ELECTION_STATE_NONE) {
            // Send election message to all replicas
            dripbox_client.election_state = ELECTION_STATE_RUNNING;
            socket_close(s);
            for (int i = 0; i < dynamic_array_length(dripbox_client.replica_sockets); i++) {
                socket_ip_tuple_t *replica_tuple = &dripbox_client.replica_sockets[i];
                struct socket *replica_sock = &replica_tuple->item1;
                if (replica_sock->sock_fd == -1 || replica_sock->error.code != 0) {
                    continue;
                }
                if (socket_pending_event(replica_sock, POLLHUP | POLLNVAL | POLLERR, 0)) {
                    socket_close(replica_sock);
                    continue;
                }
                socket_write_struct(replica_sock, ((struct dripbox_msg_header) {
                                        .version = 1,
                                        .type = DRIP_MSG_ELECTION,
                                        }), MSG_NOSIGNAL);
                diag(LOG_INFO, "Sent election to replica");
            }
            // Wait for coordinator message
            dripbox_client.election_state = ELECTION_STATE_WAITING_COORDINATOR;
            for (int i = 0; i < dynamic_array_length(dripbox_client.replica_sockets); i++) {
                socket_ip_tuple_t *replica_tuple = &dripbox_client.replica_sockets[i];
                struct socket *replica_sock = &replica_tuple->item1;
                if (replica_sock->sock_fd == -1 || replica_sock->error.code != 0) {
                    continue;
                }
                if (socket_pending_event(replica_sock, POLLHUP | POLLNVAL | POLLERR, 0)) {
                    socket_close(replica_sock);
                    continue;
                }
                dripbox_client_handle_server_message(replica_sock);
            }
        }
    }
}