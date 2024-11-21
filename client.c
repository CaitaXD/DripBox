#include <Allocator.h>
#include <stdio.h>
#include <common.h>
#include <Network.h>
#include <string.h>
#include <dripbox_common.h>
#include <inotify_common.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include  "string_view.h"
#include "array_algoritms.h"

struct string_view dripbox_username = {};
bool dripbox_client_quit = false;

const struct string_view g_sync_dir_path = (struct string_view){
    .data = "sync_dir/",
    .length = sizeof "sync_dir/" - 1,
};

static const struct string_view g_cmd_upload = (struct string_view){
    .data = "upload",
    .length = sizeof "upload" - 1,
};

static const struct string_view g_cmd_download = (struct string_view){
    .data = "download",
    .length = sizeof "download" - 1,
};

static const struct string_view g_cmd_list_client = (struct string_view){
    .data = "list_client",
    .length = sizeof "list_client" - 1,
};

static const struct string_view g_cmd_list_server = (struct string_view){
    .data = "list_server",
    .length = sizeof "list_server" - 1,
};

static const struct string_view cmd_exit = (struct string_view){
    .data = "exit",
    .length = sizeof "exit" - 1,
};

static bool fd_pending(const int fd) {
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

//struct monitor g_client_monitor = MONITOR_INITIALIZER;

static int dripbox_client_login(struct socket *s, struct string_view username);

static int dripbox_client_upload(struct socket *s, char *file_path);

static int dripbox_client_download(struct socket *s, char *file_path);

static void *dripbox_client_network_worker(const void *args);

static void dripbox_client_command_dispatch(struct socket *s, struct string_view cmd);

static void dripbox_client_handle_server_message(struct socket *s);

static int dripbox_client_list_server(struct socket *s, bool update_client_list);

void dripbox_client_list(struct string_view sync_dir_path);

void dripbox_cleint_inotify_dispatch(struct socket *s, struct inotify_event_t inotify_event);

void *inotify_watcher_worker(const void *args);

int client_main() {
    pthread_t inotify_watcher_worker_id, network_worker_id;
    struct stat dir_stat = {};
    if (stat(g_sync_dir_path.data, &dir_stat) < 0) {
        mkdir(g_sync_dir_path.data, S_IRWXU | S_IRWXG | S_IRWXO);
    }

    var server_endpoint = ipv4_endpoint_new(ntohl(ip), port);
    var s = socket_new(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!tcp_client_connect(&s, &server_endpoint)) {
        diagf(LOG_ERROR, "%s", strerror(s.error));
        return -1;
    }

    dripbox_client_login(&s, dripbox_username);
    dripbox_client_list_server(&s, true);

    pthread_create(&inotify_watcher_worker_id, NULL, (void *) inotify_watcher_worker, &s);
    pthread_create(&network_worker_id, NULL, (void *) dripbox_client_network_worker, &s);

    while (!dripbox_client_quit) {
        sleep(1);
    }

    pthread_join(network_worker_id, NULL);
    pthread_join(inotify_watcher_worker_id, NULL);

    return 0;
}

static void dripbox_client_command_dispatch(struct socket *s, const struct string_view cmd) {
    if (strncmp(cmd.data, g_cmd_upload.data, g_cmd_upload.length) == 0) {
        const var parts = sv_token(cmd, sv_cstr(" "));
        const struct string_view file_path = parts.data[1];
        if (file_path.data[file_path.length - 1] == '\n') {
            file_path.data[file_path.length - 1] = 0;
        }
        dripbox_client_upload(s, file_path.data);
    } else if (strncmp(cmd.data, g_cmd_download.data, g_cmd_download.length) == 0) {
        const var parts = sv_token(cmd, sv_cstr(" "));
        const struct string_view file_path = parts.data[1];
        if (file_path.data[file_path.length - 1] == '\n') {
            file_path.data[file_path.length - 1] = 0;
        }
        dripbox_client_download(s, file_path.data);
    } else if (strncmp(cmd.data, g_cmd_list_client.data, g_cmd_list_client.length) == 0) {
        dripbox_client_list(g_sync_dir_path);
    } else if (strncmp(cmd.data, g_cmd_list_server.data, g_cmd_list_server.length) == 0) {
        dripbox_client_list_server(s, false);
    } else if (strncmp(cmd.data, cmd_exit.data, cmd_exit.length) == 0) {
        dripbox_client_quit = true;
    }
}

void *dripbox_client_network_worker(const void *args) {
    struct socket *s = (struct socket *) args;

    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (!dripbox_client_quit) {
        s->error = 0;
        if (socket_pending(s, 0)) {
            dripbox_client_handle_server_message(s);
        }
        if (!fd_pending(STDIN_FILENO)) {
            continue;
        }

        static char buffer[1 << 20] = {};
        const struct string_view cmd = sv_cstr(fgets(buffer, sizeof buffer, stdin));
        if (cmd.length == 0) {
            continue;
        }

        dripbox_client_command_dispatch(s, cmd);
    }

    close(s->sock_fd);
    return NULL;
}

int dripbox_client_login(struct socket *s, const struct string_view username) {
    if (s->error != 0) { return -1; }

    socket_write_struct(s, ((struct dripbox_msg_header){
        .version = 1,
        .type = MSG_LOGIN,
    }), 0);

    socket_write_struct(s, ((struct dripbox_login_header){
        .length = username.length,
    }), 0);

    socket_write(s, sv_deconstruct(username), 0);

    if (s->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(s->error));
        return -1;
    }

    diagf(LOG_INFO, "Logged in\n");
    return 0;
}

int dripbox_client_upload(struct socket *s, char *file_path) {
    if (s->error != 0) { return -1; }

    struct stat st = {};
    struct string_view it = sv_cstr(file_path);
    struct string_view file_name = {};
    while (sv_split_next(&it, sv_cstr("/"), &file_name)) {}

    if (stat(file_path, &st) < 0) {
        diagf(LOG_ERROR, "%s %s\n", strerror(errno), file_path);
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        dripbox_send_error(s, EISDIR, file_path);
        return -1;
    }

    const uint8_t checksum = dripbox_file_checksum(file_path);

    diagf(LOG_INFO, "Uploaded "sv_fmt" Size=%ld Checksum: 0X%X\n",
        (int)sv_deconstruct(file_name),
        st.st_size,
        checksum
    );

    socket_write_struct(s, ((struct dripbox_msg_header) {
        .version = 1,
        .type = MSG_UPLOAD,
    }), 0);

    socket_write_struct(s, ((struct dripbox_upload_header) {
        .file_name_length = file_name.length,
        .payload_length = st.st_size,
    }), 0);

    socket_write(s, sv_deconstruct(file_name), 0);
    socket_write_struct(s, checksum, 0);

    scope(FILE *file = fopen(file_path, "rb"), file && fclose(file)) {
        if (file == NULL) {
            diagf(LOG_ERROR, "%s\n", strerror(errno));
            break;
        }
        socket_write_file(s, file, st.st_size);
    }

    if (s->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(s->error));
        return -1;
    }
    return 0;
}

int dripbox_delete_from_server(struct socket *s, char *file_path) {
    if (s->error != 0) { return -1; }

    struct string_view it = sv_cstr(file_path);
    struct string_view file_name = {};
    while (sv_split_next(&it, sv_cstr("/"), &file_name)) {}

    socket_write_struct(s, ((struct dripbox_msg_header) {
        .version = 1,
        .type = MSG_DELETE,
    }), 0);

    socket_write_struct(s, ((struct dripbox_delete_header) {
        .file_name_length = file_name.length,
    }), 0);

    socket_write(s, sv_deconstruct(file_name), 0);

    if (s->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(s->error));
        return -1;
    }

    diagf(LOG_INFO, "Sent "sv_fmt" for deletion\n", (int)sv_deconstruct(file_name));
    return 0;
}

int dripbox_client_download(struct socket *s, char *file_path) {
    if (s->error != 0) { return -1; }

    struct string_view it = SV(file_path);
    struct string_view file_name = {};
    while (sv_split_next(&it, SV("/"), &file_name)) {}

    socket_write_struct(s, ((struct dripbox_msg_header) {
        .version = 1,
        .type = MSG_DOWNLOAD,
    }), 0);

    socket_write_struct(s, ((struct dripbox_download_header) {
        .file_name_length = file_name.length,
    }), 0);

    socket_write(s, sv_deconstruct(file_name), 0);

    const var msg_header = socket_read_struct(s, struct dripbox_msg_header, 0);

    if (!dripbox_expect_version(s, msg_header.version, 1)) {
        return -1;
    }

    if (!dripbox_expect_msg(s, msg_header.type, MSG_BYTES)) {
        return -1;
    }

    const var bytes_header = socket_read_struct(s, struct dripbox_bytes_header, 0);
    diagf(LOG_INFO, "Downloading "sv_fmt" Size=%ld",
        (int)sv_deconstruct(file_name),
        bytes_header.length
    );

    return socket_redirect_to_file(s, file_path, bytes_header.length);
}

void dripbox_client_list(const struct string_view sync_dir_path) {
    struct dirent **namelist;
    int n = scandir(sync_dir_path.data, &namelist, dripbox_dirent_is_file, alphasort);

    struct stat statbuf;

    printf("\n\n==== Local Client\'s Files: ====\n\n");
    while (n--) {
        const struct string_view file_path = (struct string_view){
            .data = namelist[n]->d_name,
            .length = strlen(namelist[n]->d_name),
        };

        if (stat(path_combine(sync_dir_path, file_path).data, &statbuf) > 0) {
            const struct tm *tm_ctime = localtime(&statbuf.st_ctime);
            const struct tm *tm_atime = localtime(&statbuf.st_atime);
            const struct tm *tm_mtime = localtime(&statbuf.st_mtime);

            printf(file_times_fmt"\n",
                namelist[n]->d_name,
                tm_long_data_deconstruct(tm_ctime),
                tm_long_data_deconstruct(tm_atime),
                tm_long_data_deconstruct(tm_mtime)
            );
        }
    }
}

void dripbox_cleint_inotify_dispatch(struct socket *s, struct inotify_event_t inotify_event) {
    int i = 0;
    while (i < inotify_event.buffer_len) {
        struct inotify_event *event = (struct inotify_event *) &inotify_event.event_buffer[i];

        if (event->mask >= IN_ISDIR) { event->mask %= IN_ISDIR; }

        const struct z_string fullpath = path_combine(g_sync_dir_path, (char*)event->name);
        switch (event->mask) {
        case IN_MODIFY:
            diagf(LOG_INFO, "Modified %s\n", event->name);
            dripbox_client_upload(s, fullpath.data);
            break;
        case IN_ATTRIB:
            diagf(LOG_INFO, "%s Metadata changed \n", event->name);
            dripbox_client_upload(s, fullpath.data);
            break;
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
        default:
            printf("OTHER\n");
        }
        i += EVENT_SIZE + event->len;
    }
}

void *inotify_watcher_worker(const void *args) {
    struct socket *s = (struct socket *) args;

    const struct inotify_watcher_t watcher = init_inotify(-1, g_sync_dir_path.data);
    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (!dripbox_client_quit) {
        const struct inotify_event_t inotify_event = read_event(watcher);
        if (inotify_event.error == EAGAIN) { continue; }
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
    if (s->error != 0) { return; }

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
    diagf(LOG_INFO, "Message { Version: 0X%X Type: 0X%X }\n", msg_header.version, msg_header.type);

    if (!dripbox_expect_version(s, msg_header.version, 1)) {
        return;
    }

    diagf(LOG_ERROR, "Message Recieved: %s\n", msg_type_cstr(msg_header.type));

    switch (msg_header.type) {
    case MSG_NOOP:
        break;
    case MSG_UPLOAD: {
        dripbox_handle_server_upload(s);
        break;
    }
    case MSG_DELETE: {
        dripbox_handle_server_delete(s);
        break;
    }
    case MSG_ERROR: {
        diagf(LOG_ERROR, "Dripbox error: %s\n", dripbox_read_error(s));
        break;
    }
    default:
        break;
    }
    if (s->error != 0) { diagf(LOG_ERROR, "%s\n", strerror(s->error)); }
}

int dripbox_client_list_server(struct socket *s, const bool update_client_list) {
    if (s->error != 0) { return -1; }

    socket_write_struct(s, ((struct dripbox_msg_header) {
        .version = 1,
        .type = MSG_LIST,
    }), 0);

    const var msg_header = socket_read_struct(s, struct dripbox_msg_header, 0);

    if (!dripbox_expect_version(s, msg_header.version, 1)) {
        return -1;
    }

    if (!dripbox_expect_msg(s, msg_header.type, MSG_LIST)) {
        return -1;
    }

    const var upload_header = socket_read_struct(s, struct dripbox_list_header, 0);

    const int server_stats_count = upload_header.file_list_length;

    struct dripbox_file_stat server_stats[server_stats_count];
    socket_read_exactly(s, size_and_address(server_stats), 0);

    if(server_stats_count > 0) {
        printf("\n\n==== Client\'s server files ====\n\n");
        diagf(LOG_INFO, "Received %d files\n", server_stats_count);
    }

    for (int i = 0; i < server_stats_count; i++) {
        const struct dripbox_file_stat server_st = server_stats[i];
        time_t ctime = server_st.ctime;
        time_t atime = server_st.atime;
        time_t mtime = server_st.mtime;
        char *name = server_st.name;

        const struct tm *tm_ctime = localtime(&ctime);
        const struct tm *tm_atime = localtime(&atime);
        const struct tm *tm_mtime = localtime(&mtime);

        printf(file_times_fmt"\n", name,
            tm_long_data_deconstruct(tm_ctime),
            tm_long_data_deconstruct(tm_atime),
            tm_long_data_deconstruct(tm_mtime));
    }

    if (update_client_list) {

        struct dirent **client_entries;
        const int client_entries_count = scandir(g_sync_dir_path.data, &client_entries, dripbox_dirent_is_file, alphasort);

        struct set_item {
            struct string_view name;
            uint8_t checksum;
        };

        bool set_name_equals(const void *a, const void *b) {
            struct set_item *a_ = (struct set_item *) a;
            struct set_item *b_ = (struct set_item *) b;
            return sv_equals(a_->name, b_->name);
        }

        bool set_equals(const void *a, const void *b) {
            struct set_item *a_ = (struct set_item *) a;
            struct set_item *b_ = (struct set_item *) b;
            return sv_equals(a_->name, b_->name) && a_->checksum == b_->checksum;
        }

        var client_set = array_stack(struct set_item, client_entries_count);

        for (int i = 0; i < client_entries_count; i++) {
            struct dirent *entry = client_entries[i];
            const struct z_string path = path_combine(g_sync_dir_path, entry->d_name);
            client_set[i] = (struct set_item) {
                .name = sv_cstr(entry->d_name),
                .checksum = dripbox_file_checksum(path.data),
            };
        }

        var server_set = array_stack(struct set_item, server_stats_count);

        for (int i = 0; i < server_stats_count; i++) {
            struct dripbox_file_stat *stat = &server_stats[i];
            server_set[i] = (struct set_item) {
                .name = sv_cstr(stat->name),
                .checksum = stat->checksum,
            };
        }

        struct allocator_arena stackak_arena = allocator_stack_arena(4096);
        struct allocator *stackalloc = &stackak_arena.allocator;

        // Download = { file | file ∈ Server \ Client }
        var to_download = array_set_difference(server_set, client_set, set_equals, stackalloc);
        diagf(LOG_INFO, "To Download: %ld\n", array_length(to_download));

        // Delete = { file | file ∈ (Client ↦ file_name) \ (Server ↦ file_name) }
        var to_delete = array_set_difference(client_set, server_set, set_name_equals, stackalloc);
        diagf(LOG_INFO, "To Delete: %ld\n", array_length(to_delete));

        for (int i = 0; i < array_length(to_delete); i++) {
            struct set_item item = to_delete[i];
            const struct z_string path = path_combine(g_sync_dir_path, item.name);
            if (unlink(path.data) < 0) {
                diagf(LOG_ERROR, "%s %s\n", strerror(errno), item.name.data);
                continue;
            }
            diagf(LOG_INFO, "Deleted "sv_fmt"\n", (int)sv_deconstruct(item.name));
        }

        for (int i = 0; i < array_length(to_download); i++) {
            struct set_item item = to_download[i];
            const struct z_string path = path_combine(g_sync_dir_path, item.name);
            if(dripbox_client_download(s, path.data) < 0) {
                diagf(LOG_ERROR, "Failed to Download %s\n", path.data);
            }
        }
    }
    return 0;
}