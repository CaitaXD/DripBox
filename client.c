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

struct string_view_t username = {};
bool quit = false;

const struct string_view_t g_sync_dir_path = (struct string_view_t){
    .data = "sync_dir/",
    .length = sizeof "sync_dir/" - 1,
};

static const struct string_view_t g_cmd_upload = (struct string_view_t){
    .data = "upload",
    .length = sizeof "upload" - 1,
};

static const struct string_view_t g_cmd_download = (struct string_view_t){
    .data = "download",
    .length = sizeof "download" - 1,
};

static const struct string_view_t g_cmd_list_client = (struct string_view_t){
    .data = "list_client",
    .length = sizeof "list_client" - 1,
};

static const struct string_view_t g_cmd_list_server = (struct string_view_t){
    .data = "list_server",
    .length = sizeof "list_server" - 1,
};

static const struct string_view_t cmd_exit = (struct string_view_t){
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

static int dripbox_login_to_server(struct socket_t *s, struct string_view_t username);

static int dripbox_upload_to_server(struct socket_t *s, char *file_path);

static int dripbox_download_from_server(struct socket_t *s, char *file_path);

static bool try_recive_message(struct socket_t *s);

static void *client_network_worker(const void *args);

static int dripbox_list_server(struct socket_t *s);

void dripbox_list_client(struct string_view_t sync_dir_path);

void run_inotify_event(struct socket_t *s, struct inotify_event_t inotify_event);

void *inotify_watcher_worker(const void *args);

int client_main() {
    pthread_t inotify_watcher_thread_id, common_client_thread_id;
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
    dripbox_login_to_server(&s, username);
    diagf(LOG_INFO, "Logged in\n");
    pthread_create(&inotify_watcher_thread_id, NULL, (void *) inotify_watcher_worker, &s);
    pthread_create(&common_client_thread_id, NULL, (void *) client_network_worker, &s);

    while (!quit) {
        sleep(1);
    }

    return 0;
}

void *client_network_worker(const void *args) {
    struct socket_t *s = (struct socket_t *) args;

    while (!quit) {
        s->error = 0;
        try_recive_message(s);
        if (!fd_pending(STDIN_FILENO)) {
            continue;
        }
        static char buffer[1 << 20] = {};
        const struct string_view_t cmd = sv_cstr(fgets(buffer, sizeof buffer, stdin));
        if (cmd.length == 0) {
            continue;
        }

        if (strncmp(cmd.data, g_cmd_upload.data, g_cmd_upload.length) == 0) {
            const var parts = sv_token(cmd, sv_cstr(" "));
            const struct string_view_t file_path = parts.data[1];
            if (file_path.data[file_path.length - 1] == '\n') {
                file_path.data[file_path.length - 1] = 0;
            }
            dripbox_upload_to_server(s, file_path.data);
        } else if (strncmp(cmd.data, g_cmd_download.data, g_cmd_download.length) == 0) {
            const var parts = sv_token(cmd, sv_cstr(" "));
            const struct string_view_t file_path = parts.data[1];
            if (file_path.data[file_path.length - 1] == '\n') {
                file_path.data[file_path.length - 1] = 0;
            }
            dripbox_download_from_server(s, file_path.data);
        } else if (strncmp(cmd.data, g_cmd_list_client.data, g_cmd_list_client.length) == 0) {
            dripbox_list_client(g_sync_dir_path);
        } else if (strncmp(cmd.data, g_cmd_list_server.data, g_cmd_list_server.length) == 0) {
            dripbox_list_server(s);
        } else if (strncmp(cmd.data, cmd_exit.data, cmd_exit.length) == 0) {
            quit = true;
        }
    }

    close(s->sock_fd);
    return NULL;
}

int dripbox_login_to_server(struct socket_t *s, const struct string_view_t username) {
    if (s->error != 0) { return -1; }

    socket_write_struct(s, ((struct dripbox_msg_header_t){
        .version = 1,
        .type = MSG_LOGIN,
    }), 0);

    socket_write_struct(s, ((struct dripbox_login_header_t){
        .length = username.length,
    }), 0);

    socket_write(s, sv_deconstruct(username), 0);

    if (s->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(s->error));
        return -1;
    }
    return 0;
}

int dripbox_upload_to_server(struct socket_t *s, char *file_path) {
    if (s->error != 0) { return -1; }

    struct stat file_stat = {};
    struct string_view_t it = sv_cstr(file_path);
    struct string_view_t file_name = {};
    while (sv_split_next(&it, sv_cstr("/"), &file_name)) {}

    if (stat(file_path, &file_stat) < 0) {
        diagf(LOG_ERROR, "%s %s\n", strerror(errno), file_path);
        return -1;
    }

    const uint8_t checksum = dripbox_file_checksum(file_path);

    diagf(LOG_INFO, "Sending File "sv_fmt" Checksum %d\n", (int)sv_deconstruct(file_name), checksum);

    const struct dripbox_msg_header_t msg_header = {
        .version = 1,
        .type = MSG_UPLOAD,
    };

    const struct dripbox_upload_header_t upload_header = {
        .file_name_length = file_name.length,
        .payload_length = file_stat.st_size,
    };

    socket_write_struct(s, msg_header, 0);
    socket_write_struct(s, upload_header, 0);
    socket_write(s, sv_deconstruct(file_name), 0);
    socket_write_struct(s, checksum, 0);

    scope(FILE *file = fopen(file_path, "rb"), file && fclose(file)) {
        if (file == NULL) {
            diagf(LOG_ERROR, "%s\n", strerror(errno));
            break;
        }
        socket_write_file(s, file, file_stat.st_size);
    }

    if (s->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(s->error));
        return -1;
    }
    return 0;
}

int dripbox_delete_from_server(struct socket_t *s, char *file_path) {
    if (s->error != 0) { return -1; }

    struct string_view_t it = sv_cstr(file_path);
    struct string_view_t file_name = {};
    while (sv_split_next(&it, sv_cstr("/"), &file_name)) {
    }

    const struct dripbox_msg_header_t msg_header = {
        .version = 1,
        .type = MSG_DELETE,
    };

    const struct dripbox_delete_header_t delete_header = {
        .file_name_length = file_name.length,
    };

    socket_write(s, size_and_address(msg_header), 0);
    socket_write(s, size_and_address(delete_header), 0);
    socket_write(s, sv_deconstruct(file_name), 0);

    if (s->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(s->error));
        return -1;
    }

    diagf(LOG_INFO, "Sent File "sv_fmt" to delete\n", (int)sv_deconstruct(file_name));
    return 0;
}

int dripbox_download_from_server(struct socket_t *s, char *file_path) {
    if (s->error != 0) { return -1; }

    struct string_view_t it = sv_cstr(file_path);
    struct string_view_t file_name = {};
    while (sv_split_next(&it, sv_cstr("/"), &file_name)) {
    }

    const struct dripbox_msg_header_t in_msg_header = {
        .version = 1,
        .type = MSG_DOWNLOAD,
    };

    const struct dripbox_download_header_t download_header = {
        .file_name_length = file_name.length,
    };

    socket_write_struct(s, in_msg_header, 0);
    socket_write_struct(s, download_header, 0);
    socket_write(s, sv_deconstruct(file_name), 0);
    if (s->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(s->error));
        return -1;
    }

    const struct dripbox_msg_header_t out_msg_header = socket_read_struct(s, struct dripbox_msg_header_t, 0);
    if (s->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(errno));
        return -1;
    }

    if (out_msg_header.type == MSG_ERROR) {
        const struct dripbox_error_header_t error_header = socket_read_struct(s, struct dripbox_error_header_t, 0);
        const struct string_view_t error_msg = (struct string_view_t){
            .data = socket_read_array(s, char, error_header.error_length, 0).data,
            .length = error_header.error_length,
        };
        diagf(LOG_ERROR, "Dripbox error: "sv_fmt"\n", (int) sv_deconstruct(error_msg));
        return -1;
    }

    const struct dripbox_upload_header_t upload_header = socket_read_struct(s, struct dripbox_upload_header_t, 0);
    if (s->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(errno));
        return -1;
    }

    return socket_redirect_to_file(s, file_path, upload_header.payload_length);
}

void dripbox_list_client(const struct string_view_t sync_dir_path) {
    struct dirent **namelist;
    int n;
    int filter(const struct dirent *name) {
        if (name->d_type == DT_DIR) {
            return 0;
        }
        return 1;
    }

    n = scandir(sync_dir_path.data, &namelist, filter, alphasort);

    struct stat statbuf;

    printf("\n\n*****LOCAL CLIENT\'S FILES:*****\n\n");
    while (n--) {
        const struct string_view_t file_path = (struct string_view_t){
            .data = namelist[n]->d_name,
            .length = strlen(namelist[n]->d_name),
        };
        int status = stat(path_combine(sync_dir_path, file_path).data, &statbuf);

        if (!status) {
            struct tm *tm_ctime = localtime(&statbuf.st_ctime);
            struct tm *tm_atime = localtime(&statbuf.st_atime);
            struct tm *tm_mtime = localtime(&statbuf.st_mtime);

            printf("NAME: %s \n", namelist[n]->d_name);
            printf("CTIME: %d/%2d/%2d %2d:%2d.%2d\n", tm_ctime->tm_year + 1900, tm_ctime->tm_mon + 1, tm_ctime->tm_mday,
                   tm_ctime->tm_hour, tm_ctime->tm_min, tm_ctime->tm_sec);
            printf("ATIME: %d/%2d/%2d %2d:%2d.%2d\n", tm_atime->tm_year + 1900, tm_atime->tm_mon + 1, tm_atime->tm_mday,
                   tm_atime->tm_hour, tm_atime->tm_min, tm_atime->tm_sec);
            printf("MTIME: %d/%2d/%2d %2d:%2d.%2d\n", tm_mtime->tm_year + 1900, tm_mtime->tm_mon + 1, tm_mtime->tm_mday,
                   tm_mtime->tm_hour, tm_mtime->tm_min, tm_mtime->tm_sec);
            printf("\n");
        }
    }
    printf("*******************************\n\n");
}

void run_inotify_event(struct socket_t *s, struct inotify_event_t inotify_event) {
    int i = 0;
    while (i < inotify_event.buffer_len) {
        struct inotify_event *event = (struct inotify_event *) &inotify_event.event_buffer[i];

        if (event->mask >= IN_ISDIR) { event->mask %= IN_ISDIR; }

        diagf(LOG_INFO, "File Name: "sv_fmt"\n", event->len, event->name);
        const struct z_string_t fullpath = path_combine(g_sync_dir_path, (char*)event->name);
        switch (event->mask) {
        case IN_MODIFY:
            printf("File modified %s\n", event->name);
            dripbox_upload_to_server(s, fullpath.data);
            break;
        case IN_ATTRIB:
            printf("File Metadata changed %s\n", event->name);
            dripbox_upload_to_server(s, fullpath.data);
            break;
        case IN_MOVED_TO:
            printf("File modified or created %s\n", event->name);
            dripbox_upload_to_server(s, fullpath.data);
            break;
        case IN_MOVED_FROM:
            printf("File moved %s\n", event->name);
            dripbox_delete_from_server(s, event->name);
            break;
        case IN_DELETE:
            printf("File deleted %s\n", event->name);
            dripbox_delete_from_server(s, event->name);
            break;
        case IN_DELETE_SELF:
            printf("[NOT IMPLEMENTED] IN_DELETE_SELF\n");
            break;
        default:
            printf("OTHER\n");
        }
        i += EVENT_SIZE + event->len;
    }
}

void *inotify_watcher_worker(const void *args) {
    struct socket_t *s = (struct socket_t *) args;

    const struct inotify_watcher_t watcher = init_inotify(-1, g_sync_dir_path.data);
    while (!quit) {
        const struct inotify_event_t inotify_event = read_event(watcher);
        if (inotify_event.error == EAGAIN) { continue; }
        run_inotify_event(s, inotify_event);
    }

    const int ret_value = inotify_rm_watch(watcher.inotify_fd, watcher.watcher_fd);
    if (ret_value) {
        diagf(LOG_ERROR, "Inotify watch remove: %s\n", strerror(errno));
        return (void *) -1;
    }

    return 0;
}

void dripbox_delete_from_client(struct socket_t *s) {
    const var delete_header = socket_read_struct(s, struct dripbox_delete_header_t, 0);
    const struct string_view_t file_name = {
        .data = socket_read_array(s, char, delete_header.file_name_length, 0).data,
        .length = delete_header.file_name_length,
    };
    if (s->error != 0) { return; }

    const struct z_string_t path = path_combine(g_sync_dir_path, file_name);
    if (unlink(path.data) < 0) {
        diagf(LOG_ERROR, "%s\n", strerror(errno));
        return;
    }
    diagf(LOG_INFO, "Deleted "sv_fmt"\n", (int)sv_deconstruct(file_name));
}

void dripbox_handle_upload_to_client(struct socket_t *s) {
    const var upload_header = socket_read_struct(s, struct dripbox_upload_header_t, 0);
    const struct string_view_t file_name = {
        .data = socket_read_array(s, char, upload_header.file_name_length, 0).data,
        .length = upload_header.file_name_length,
    };
    const uint8_t server_checksum = socket_read_struct(s, uint8_t, 0);
    const var path = path_combine(g_sync_dir_path, file_name);
    if (stat(path.data, &(struct stat){}) < 0) { goto send_file; }

    const uint8_t checksum = dripbox_file_checksum(path.data);
    if (checksum == server_checksum) {
        diagf(LOG_INFO, "No changes in "sv_fmt" discarding\n", (int)sv_deconstruct(file_name));
        socket_redirect_to_file(s, "/dev/null", upload_header.payload_length);
        return;
    }
send_file:
    if (socket_redirect_to_file(s, path.data, upload_header.payload_length) != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(errno));
    }
}

void recive_message(struct socket_t *s) {
    const var msg_header = socket_read_struct(s, struct dripbox_msg_header_t, 0);
    diagf(LOG_INFO, "Recieved Message %s\n", msg_type_cstr(msg_header.type));
    switch (msg_header.type) {
    case MSG_NOOP:
        break;
    case MSG_UPLOAD: {
        dripbox_handle_upload_to_client(s);
        break;
    }
    case MSG_DELETE: {
        dripbox_delete_from_client(s);
        break;
    }
    case MSG_ERROR: {
        const var error_header = socket_read_struct(s, struct dripbox_error_header_t, 0);
        const struct string_view_t error_msg = {
            .data = socket_read_array(s, char, error_header.error_length, 0).data,
            .length = error_header.error_length,
        };
        diagf(LOG_ERROR, "Dripbox error: "sv_fmt"\n", (int) sv_deconstruct(error_msg));
        break;
    }
    default:
        diagf(LOG_ERROR, "Type: 0x%X\n", msg_header.type);
        break;
    }
    if (s->error != 0) { diagf(LOG_ERROR, "%s\n", strerror(s->error)); }
}

bool try_recive_message(struct socket_t *s) {
    if (!socket_pending(s, 0)) { return false; }
    recive_message(s);
    return true;
}

int dripbox_list_server(struct socket_t *s) {
    if (s->error != 0) { return -1; }

    const struct dripbox_msg_header_t in_msg_header = {
        .version = 1,
        .type = MSG_LIST,
    };

    socket_write(s, size_and_address(in_msg_header), 0);
    if (s->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(s->error));
        return -1;
    }

    const var out_msg_header = socket_read_struct(s, struct dripbox_msg_header_t, 0);

    if (out_msg_header.type == MSG_ERROR) {
        const var error_header = socket_read_struct(s, struct dripbox_error_header_t, 0);
        const struct string_view_t error_msg = {
            .data = socket_read_array(s, char, error_header.error_length, 0).data,
            .length = error_header.error_length,
        };
        diagf(LOG_ERROR, "Dripbox error: "sv_fmt"\n", (int) sv_deconstruct(error_msg));
        return -1;
    }

    const var upload_header = socket_read_struct(s, struct dripbox_upload_header_t, 0);

    int n = upload_header.payload_length;
    struct dripbox_file_times_t list_files[n];
    memset(list_files, 0, n * sizeof(struct dripbox_file_times_t));
    socket_read_exactly(s, size_and_address(list_files), 0);

    if (s->error != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(errno));
        return -1;
    }

    printf("\n\n*****CLIENT\'S SERVER FILES:*****\n\n");
    while (n && n--) {
        time_t ctime = list_files[n].ctime;
        time_t atime = list_files[n].atime;
        time_t mtime = list_files[n].mtime;
        char name[256] = {};

        memcpy(name, list_files[n].name, strlen(list_files[n].name) + 1);
        const struct tm *tm_ctime = localtime(&ctime);
        const struct tm *tm_atime = localtime(&atime);
        const struct tm *tm_mtime = localtime(&mtime);

        printf("NAME: %s \n", name);
        printf("CTIME: %d/%2d/%2d %2d:%2d.%2d\n", tm_ctime->tm_year + 1900, tm_ctime->tm_mon + 1, tm_ctime->tm_mday,
               tm_ctime->tm_hour, tm_ctime->tm_min, tm_ctime->tm_sec);
        printf("ATIME: %d/%2d/%2d %2d:%2d.%2d\n", tm_atime->tm_year + 1900, tm_atime->tm_mon + 1, tm_atime->tm_mday,
               tm_atime->tm_hour, tm_atime->tm_min, tm_atime->tm_sec);
        printf("MTIME: %d/%2d/%2d %2d:%2d.%2d\n", tm_mtime->tm_year + 1900, tm_mtime->tm_mon + 1, tm_mtime->tm_mday,
               tm_mtime->tm_hour, tm_mtime->tm_min, tm_mtime->tm_sec);
        printf("\n");
    }

    return 0;
}
