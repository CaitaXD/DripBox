#include <Allocator.h>
#include <stdio.h>
#include <common.h>
#include <Network.h>
#include <string.h>
#include <dripbox_common.h>
#include <inotify_common.h>
#include <sys/sendfile.h>
#include  "string_view.h"

struct string_view_t username = {};

static int dripbox_login(struct socket_t *s, struct string_view_t username);

static int dripbox_upload(struct socket_t *s, char *file_path);

static int dripbox_download(struct socket_t *s, char *file_path);

static void *client_rotine(const void *args);

void run_inotify_event(struct socket_t *s, struct inotify_event_t inotify_event);

void *inotify_watcher_loop(const void *args);

int client_main() {
    pthread_t inotify_watcher_thread_id, common_client_thread_id;
    const struct string_view_t sync_dir_path = (struct string_view_t){
        .data = "./sync_dir/",
        .length = sizeof "./sync_dir/" - 1,
    };
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

    pthread_create(&inotify_watcher_thread_id, NULL, (void *) inotify_watcher_loop, &s);
    pthread_create(&common_client_thread_id, NULL, (void *) client_rotine, &s);
    while (1) {
        sleep(1);
    }
    return 0;
}

void *client_rotine(const void *args) {
    struct socket_t *s = (struct socket_t *) args;

    const struct string_view_t sync_dir_path = (struct string_view_t){
        .data = "./sync_dir/",
        .length = sizeof "./sync_dir/" - 1,
    };

    static char buffer[1 << 20] = {};
    static const struct string_view_t cmd_upload = (struct string_view_t){
        .data = "upload",
        .length = sizeof "upload" - 1,
    };
    static const struct string_view_t cmd_download = (struct string_view_t){
        .data = "download",
        .length = sizeof "download" - 1,
    };

    bool quit = false;
    while (!quit) {
        s->error = 0;
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

            dripbox_download(s, path_combine(&default_allocator, sync_dir_path, file_path).data);
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
    sent += socket_write(s, sv_args(username), 0);

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
    sent += socket_write(s, sv_args(file_name), 0);

    scope(FILE *file = fopen(file_path, "rb"), fclose(file)) {
        if (file == NULL) {
            log(LOG_ERROR, "%s\n", strerror(errno));
            break;
        }
        sent += socket_read_file(s, file, file_stat.st_size);
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
    sent += socket_write(s, sv_args(file_name), 0);

    if (s->error != 0) {
        log(LOG_ERROR, "%s\n", strerror(s->error));
        return -1;
    }
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

        const struct z_string_t fullpath = path_combine(&default_allocator, SV("sync_dir"), SV(event->name));

        switch (event->mask) {
        case IN_MODIFY: printf("File modified or created %s\n", event->name);
            dripbox_upload(s, fullpath.data);
            break;
        case IN_ATTRIB: printf("[NOT IMPLEMENTED] IN_ATTRIB\n");
            break;
        case IN_MOVED_TO: printf("[NOT IMPLEMENTED] IN_MOVED_TO\n");
            break;
        case IN_MOVED_FROM: printf("[NOT IMPLEMENTED] IN_MOVED_FROM\n");
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

void *inotify_watcher_loop(const void *args) {
    struct socket_t *s = (struct socket_t *) args;

    const struct inotify_watcher_t watcher = init_inotify(-1, "./sync_dir");
    //fcntl(watcher.inotify_fd, F_SETFL, O_NONBLOCK);
    while (true) {
        const struct inotify_event_t inotify_event = read_event(watcher);
        if (inotify_event.error == EAGAIN) { continue; }
        run_inotify_event(s, inotify_event);
    }

    int ret_value = inotify_rm_watch(watcher.inotify_fd, watcher.watcher_fd);
    if (ret_value) {
        log(LOG_ERROR, "Inotify watch remove: %s\n", strerror(errno));
        return (void *) -1;
    }

    return 0;
}
