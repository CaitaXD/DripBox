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

const struct string_view_t sync_dir_path = (struct string_view_t){
    .data = "sync_dir/",
    .length = sizeof "sync_dir/" - 1,
};
static const struct string_view_t cmd_upload = (struct string_view_t){
    .data = "upload",
    .length = sizeof "upload" - 1,
};
static const struct string_view_t cmd_download = (struct string_view_t){
    .data = "download",
    .length = sizeof "download" - 1,
};
static const struct string_view_t cmd_list_client = (struct string_view_t){
    .data = "list_client",
    .length = sizeof "list_client" - 1,
};
static const struct string_view_t cmd_list_server = (struct string_view_t){
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

static int dripbox_login(struct socket_t *s, struct string_view_t username);

static int dripbox_upload(struct socket_t *s, char *file_path);

static int dripbox_download(struct socket_t *s, char *file_path);

static void try_recive_message(struct socket_t *s);

static void *client_rotine(const void *args);

void dripbox_list_client(struct string_view_t sync_dir_path);

void run_inotify_event(struct socket_t *s, struct inotify_event_t inotify_event);

void *inotify_watcher_loop(const void *args);

int client_main() {
    pthread_t inotify_watcher_thread_id, common_client_thread_id;
    struct stat dir_stat = {};
    if (stat(sync_dir_path.data, &dir_stat) < 0) {
        mkdir(sync_dir_path.data, S_IRWXU | S_IRWXG | S_IRWXO);
    }

    var server_endpoint = ipv4_endpoint_new(ntohl(ip), port);
    var s = socket_new(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!tcp_client_connect(&s, &server_endpoint)) {
        log(LOG_ERROR, "%s", strerror(s.error));
        return -1;
    }
    dripbox_login(&s, username);
    log(LOG_INFO, "Logged in\n");
    pthread_create(&inotify_watcher_thread_id, NULL, (void *) inotify_watcher_loop, &s);
    pthread_create(&common_client_thread_id, NULL, (void *) client_rotine, &s);

    while (!quit) {
        sleep(1);
    }

    return 0;
}

void *client_rotine(const void *args) {
    struct socket_t *s = (struct socket_t *) args;

    while (!quit) {
        try_recive_message(s);
        s->error = 0;
        if (!fd_pending(STDIN_FILENO)) {
            continue;
        }
        static char buffer[1 << 20] = {};
        const struct string_view_t cmd = sv_cstr(fgets(buffer, sizeof buffer, stdin));
        if (cmd.length == 0) {
            continue;
        }

        if (strncmp(cmd.data, cmd_upload.data, cmd_upload.length) == 0) {
            const var parts = sv_token(cmd, sv_cstr(" "));
            const struct string_view_t file_path = parts.data[1];
            if (file_path.data[file_path.length - 1] == '\n') {
                file_path.data[file_path.length - 1] = 0;
            }
            dripbox_upload(s, file_path.data);
        } else if (strncmp(cmd.data, cmd_download.data, cmd_download.length) == 0) {
            const var parts = sv_token(cmd, sv_cstr(" "));
            const struct string_view_t file_path = parts.data[1];
            if (file_path.data[file_path.length - 1] == '\n') {
                file_path.data[file_path.length - 1] = 0;
            }
            dripbox_download(s, file_path.data);
        } else if (strncmp(cmd.data, cmd_list_client.data, cmd_list_client.length) == 0) {
            dripbox_list_client(sync_dir_path);
        } else if (strncmp(cmd.data, cmd_list_server.data, cmd_list_server.length) == 0) {
            printf("list_server not implemented\n\n");
            log(LOG_ERROR, "%s\n", "NOT IMPLEMENTED");
        } else if (strncmp(cmd.data, cmd_exit.data, cmd_exit.length) == 0) {
            quit = true;
        }
    }

    close(s->sock_fd);
    return NULL;
}

int dripbox_login(struct socket_t *s, const struct string_view_t username) {
    if (s->error != 0) { return -1; }

    const struct dripbox_msg_header_t msg_header = {
        .version = 1,
        .type = MSG_LOGIN,
    };

    const struct dripbox_login_header_t login_msg_header = {
        .length = username.length,
    };

    ssize_t sent = 0;
    sent += socket_write(s, size_and_address(msg_header), 0);
    sent += socket_write(s, size_and_address(login_msg_header), 0);
    sent += socket_write(s, sv_deconstruct(username), 0);

    if (s->error != 0) {
        log(LOG_ERROR, "%s\n", strerror(s->error));
        return -1;
    }
    return sent;
}

int dripbox_upload(struct socket_t *s, char *file_path) {
    if (s->error != 0) { return -1; }

    struct stat file_stat = {};
    struct string_view_t it = sv_cstr(file_path);
    struct string_view_t file_name = {};
    while (sv_split_next(&it, sv_cstr("/"), &file_name)) {
    }

    if (stat(file_path, &file_stat) < 0) {
        log(LOG_ERROR, "%s %s\n", strerror(errno), file_path);
        return -1;
    }

    const struct dripbox_msg_header_t msg_header = {
        .version = 1,
        .type = MSG_UPLOAD,
    };

    const struct dripbox_upload_header_t upload_header = {
        .file_name_length = file_name.length,
        .payload_length = file_stat.st_size,
    };

    ssize_t sent = 0;

    sent += socket_write(s, size_and_address(msg_header), 0);
    sent += socket_write(s, size_and_address(upload_header), 0);
    sent += socket_write(s, sv_deconstruct(file_name), 0);

    scope(FILE *file = fopen(file_path, "rb"), fclose(file)) {
        if (file == NULL) {
            log(LOG_ERROR, "%s\n", strerror(errno));
            break;
        }
        sent += socket_write_file(s, file, file_stat.st_size);
    }
    if (s->error != 0) {
        log(LOG_ERROR, "%s\n", strerror(s->error));
        return -1;
    }
    return sent;
}

int dripbox_delete(struct socket_t *s, char *file_path) {
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

    ssize_t sent = 0;

    sent += socket_write(s, size_and_address(msg_header), 0);
    sent += socket_write(s, size_and_address(delete_header), 0);
    sent += socket_write(s, sv_deconstruct(file_name), 0);

    if (s->error != 0) {
        log(LOG_ERROR, "%s\n", strerror(s->error));
        return -1;
    }

    log(LOG_INFO, "Sent File %*s to delete\n", (int)sv_deconstruct(file_name));
    return sent;
}

int dripbox_download(struct socket_t *s, char *file_path) {
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

    socket_write(s, size_and_address(in_msg_header), 0);
    socket_write(s, size_and_address(download_header), 0);
    socket_write(s, download_header.file_name_length, (uint8_t *) file_name.data, 0);
    if (s->error != 0) {
        log(LOG_ERROR, "%s\n", strerror(s->error));
        return -1;
    }
    uint8_t buffer[DRIPBOX_MAX_HEADER_SIZE] = {};

    struct dripbox_msg_header_t *out_msg_header = (void *) buffer;
    if (socket_read_exactly(s, size_and_address(*out_msg_header), 0) < 0) {
        log(LOG_ERROR, "%s\n", strerror(errno));
        return -1;
    }

    if (out_msg_header->type == MSG_ERROR) {
        var error_header = *(struct dripbox_error_header_t *) (buffer + sizeof *out_msg_header);
        uint8_t *error_msg_buffer = buffer + sizeof error_header;

        socket_read_exactly(s, size_and_address(error_header), 0);
        socket_read_exactly(s, error_header.error_length, error_msg_buffer, 0);
        if (s->error != 0) {
            log(LOG_ERROR, "%s\n", strerror(s->error));
            printf("Dripbox error: Unknown\n");
        } else {
            printf("Dripbox error: %.*s\n", (int) error_header.error_length, (char *) error_msg_buffer);
        }
        return -1;
    }

    var upload_header = *(struct dripbox_upload_header_t *) (buffer + sizeof *out_msg_header);
    uint8_t *file_name_buffer = buffer + sizeof upload_header;

    socket_read_exactly(s, size_and_address(upload_header), 0);
    socket_read_exactly(s, upload_header.file_name_length, file_name_buffer, 0);

    if (s->error != 0) {
        log(LOG_ERROR, "%s\n", strerror(errno));
        return -1;
    }

    ssize_t got = 0;
    scope(FILE *file = fopen(file_path, "wb"), fclose(file)) {
        ssize_t length = upload_header.payload_length;
        while (length > 0) {
            const ssize_t result = recv(s->sock_fd, buffer, DRIPBOX_MAX_HEADER_SIZE, 0);
            if (result == 0) { break; }
            if (result < 0) {
                log(LOG_ERROR, "%s\n", strerror(errno));
                got = -1;
                break;
            }
            length -= result;
            got += result;
            if (fwrite(buffer, sizeof(uint8_t), result, file) < 0) {
                log(LOG_ERROR, "%s\n", strerror(errno));
                got = -1;
                break;
            }
        }
    }

    return got;
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

    // referencia do scandir pro relatorio dps: https://lloydrochester.com/post/c/list-directory/
    n = scandir(sync_dir_path.data, &namelist, filter, alphasort);

    struct stat statbuf;

    printf("\n\n*****LOCAL CLIENT\'S FILES:*****\n\n");
    while (n--) {
        const struct string_view_t file_path = (struct string_view_t){
            .data = namelist[n]->d_name,
            .length = strlen(namelist[n]->d_name),
        };
        // referencia do stat pro relatorio: https://pubs.opengroup.org/onlinepubs/009695399/functions/stat.html
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

        printf("wd=%d mask=%u cookie=%u len=%u\n",
               event->wd, event->mask,
               event->cookie, event->len);

        if (event->len)
            printf("name=%s\n", event->name);

        // dir action (any action)
        if (event->mask >= IN_ISDIR) {
            printf("dir action\n");
            event->mask %= IN_ISDIR;
        }

        const struct z_string_t fullpath = path_combine("sync_dir", (char*)event->name);

        switch (event->mask) {
        case IN_MODIFY: printf("File modified or created %s\n", event->name);
            dripbox_upload(s, fullpath.data);
            break;
        case IN_ATTRIB: printf("File modified or created %s\n", event->name);
            dripbox_upload(s, fullpath.data);
            break;
        case IN_MOVED_TO: printf("File modified or created %s\n", event->name);
            dripbox_upload(s, fullpath.data);
            break;
        case IN_MOVED_FROM: printf("File deleted %s\n", event->name);
            dripbox_delete(s, event->name);
            break;
        case IN_DELETE: printf("File deleted %s\n", event->name);
            dripbox_delete(s, event->name);
            break;
        case IN_DELETE_SELF: printf("[NOT IMPLEMENTED] IN_DELETE_SELF\n");
            break;
        default: printf("OTHER\n");
        }
        i += EVENT_SIZE + event->len;
    }
}


const struct inotify_watcher_t watcher;

void *inotify_watcher_loop(const void *args) {
    struct socket_t *s = (struct socket_t *) args;

    watcher = init_inotify(-1, "sync_dir");
    while (!quit) {
        const struct inotify_event_t inotify_event = read_event(watcher);
        if (inotify_event.error != 0) {
            continue;
        }
        run_inotify_event(s, inotify_event);
    }

    const int ret_value = inotify_rm_watch(watcher.inotify_fd, watcher.watcher_fd);
    if (ret_value) {
        log(LOG_ERROR, "Inotify watch remove: %s\n", strerror(errno));
        return (void *) -1;
    }

    return 0;
}

void recive_message(struct socket_t *s) {
    inotify_rm_watch(watcher.inotify_fd, watcher.watcher_fd);
    uint8_t buffer[DRIPBOX_MAX_HEADER_SIZE] = {};
    const struct dripbox_msg_header_t *msg_header = (void *) buffer;
    if (socket_read_exactly(s, size_and_address(*msg_header), 0) == 0) {
        return;
    }
    if (s->error != 0) {
        log(LOG_ERROR, "%s\n", strerror(s->error));
        return;
    }

    switch (msg_header->type) {
    case MSG_NOOP: break;
    case MSG_UPLOAD: {
        const struct dripbox_upload_header_t *upload_header = (void *) (buffer + sizeof *msg_header);
        socket_read_exactly(s, size_and_address(*upload_header), 0);
        socket_read_exactly(s, upload_header->file_name_length, (uint8_t *) upload_header + sizeof *upload_header, 0);
        if (s->error != 0) {
            log(LOG_ERROR, "%s\n", strerror(s->error));
            return;
        }

        const struct string_view_t file_name = sv_take(
            sv_cstr((char *) upload_header + sizeof *upload_header),
            upload_header->file_name_length
        );

        const var path = path_combine("sync_dir", file_name);
        if (socket_read_to_file(s, path.data, upload_header->payload_length) != 0) {
            log(LOG_ERROR, "%s\n", strerror(errno));
        }
        break;
    }
    case MSG_DELETE: {
        const struct dripbox_delete_header_t *delete_header = (void *) msg_header + sizeof *msg_header;
        socket_read_exactly(s, size_and_address(*delete_header), 0);
        socket_read_exactly(s, delete_header->file_name_length, (uint8_t *) delete_header + sizeof *delete_header, 0);
        if (s->error != 0) {
            log(LOG_ERROR, "%s\n", strerror(s->error));
            return;
        }

        const struct string_view_t file_name = sv_take(
            sv_cstr((char *) delete_header + sizeof *delete_header),
            delete_header->file_name_length
        );

        if (unlink(path_combine("sync_dir", file_name).data) < 0) {
            log(LOG_ERROR, "%s\n", strerror(errno));
        }
        break;
    }
    default: {
        log(LOG_ERROR, "Invalid Message Type %s\n %d", msg_type_cstr(msg_header->type), msg_header->type);
        break;
    }
    }
    watcher = init_inotify(-1, "sync_dir");
}

void try_recive_message(struct socket_t *s) {
    if (!socket_pending(s, 0)) { return; }
    recive_message(s);
}
