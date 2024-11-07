#include <Allocator.h>
#include <stdio.h>
#include <common.h>
#include <Network.h>
#include <string.h>
#include <dripbox_common.h>
#include "string_view.h"

struct string_view_t username = {};

static int dripbox_login(struct socket_t *s, struct string_view_t username);

static int dripbox_upload(struct socket_t *s, char *file_path);

static int dripbox_download(struct socket_t *s, char *file_path);

int client_main() {
    var server_endpoint = ipv4_endpoint_new(ip, port);
    var s = socket_new(AF_INET);
    if (!tcp_client_connect(&s, &server_endpoint)) {
        log(LOG_ERROR, "%s", strerror(s.error));
        return -1;
    }
    dripbox_login(&s, username);

    const struct string_view_t sync_dir_path = (struct string_view_t){
        .data = "./sync_dir/",
        .length = sizeof "./sync_dir/" - 1,
    };
    struct stat dir_stat = {};
    if (stat(sync_dir_path.data, &dir_stat) < 0) {
        mkdir(sync_dir_path.data, S_IRWXU | S_IRWXG | S_IRWXO);
    }

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
        s.error = 0;
        const struct string_view_t cmd = sv_from_cstr(fgets(buffer, sizeof buffer, stdin));
        if (cmd.length == 0) {
            continue;
        }

        if (strncmp(cmd.data, cmd_upload.data, cmd_upload.length) == 0) {
            const var parts = sv_token(cmd, sv_from_cstr(" "));
            const struct string_view_t file_path = parts.data[1];
            if (file_path.data[file_path.length - 1] == '\n') {
                file_path.data[file_path.length - 1] = 0;
            }
            dripbox_upload(&s, file_path.data);
        } else if (strncmp(cmd.data, cmd_download.data, cmd_download.length) == 0) {
            const var parts = sv_token(cmd, sv_from_cstr(" "));
            const struct string_view_t file_path = parts.data[1];
            if (file_path.data[file_path.length - 1] == '\n') {
                file_path.data[file_path.length - 1] = 0;
            }

            dripbox_download(&s, path_combine(tls_dripbox_path_buffer, sync_dir_path.data, file_path.data));
        }
    }

    close(s.sock_fd);
    return 0;
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
    struct string_view_t it = sv_from_cstr(file_path);
    struct string_view_t file_name = {};
    while (sv_split_next(&it, sv_from_cstr("/"), &file_name)) {
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

int dripbox_download(struct socket_t *s, char *file_path) {
    if (s->error != 0) { return -1; }

    struct string_view_t it = sv_from_cstr(file_path);
    struct string_view_t file_name = {};
    while (sv_split_next(&it, sv_from_cstr("/"), &file_name)) {
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
    if (socket_read_exactly(s, size_and_address(*out_msg_header)) < 0) {
        log(LOG_ERROR, "%s\n", strerror(errno));
        return -1;
    }

    if (out_msg_header->type == MSG_ERROR) {
        var error_header = *(struct dripbox_error_header_t *) (buffer + sizeof *out_msg_header);
        uint8_t *error_msg_buffer = buffer + sizeof error_header;

        socket_read_exactly(s, size_and_address(error_header));
        socket_read_exactly(s, error_header.error_length, error_msg_buffer);
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

    socket_read_exactly(s, size_and_address(upload_header));
    socket_read_exactly(s, upload_header.file_name_length, file_name_buffer);

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
