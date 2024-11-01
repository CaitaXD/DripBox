#include <Allocator.h>
#include <stdio.h>
#include <common.h>
#include <Network.h>
#include <string.h>
#include <dripbox_common.h>
#include<sys/sendfile.h>

struct string_view_t username = {};

static int send_dripbox_payload(const struct socket_t *s, const struct dripbox_login_header_t header,
                                uint8_t data[header.length]) {
    int sent = 0, ret;

    if ((ret = send(s->sock_fd, &header, sizeof header, 0)) < 0) {
        log(LOG_ERROR, "Error sending header: %s\n", strerror(s->last_error));
        return -1;
    }
    sent += ret;

    if ((ret = send(s->sock_fd, data, header.length, 0)) < 0) {
        log(LOG_ERROR, "Error sending data: %s\n", strerror(s->last_error));
        return -1;
    }
    sent += ret;
    return sent;
}

static int dripbox_login(const struct socket_t *s, const struct string_view_t username) {
    const struct dripbox_login_header_t login_msg_header = {
        .version = 1,
        .type = MSG_LOGIN,
        .length = username.length,
    };
    return send_dripbox_payload(s, login_msg_header, (uint8_t *) username.data);
}

static int dripbox_upload(const struct socket_t *s, char *file_path) {
    struct stat file_stat = {0};
    struct string_view_t it = sv_from_cstr(file_path);
    struct string_view_t file_name = {};
    while (sv_split_next(&it, sv_from_cstr("/"), &file_name)) {
    }

    if (stat(file_path, &file_stat) < 0) {
        log(LOG_ERROR, "%s %s\n", strerror(errno), file_path);
        return -1;
    }

    const struct dripbox_upload_header_t upload_header = {
        .version = 1,
        .type = MSG_UPLOAD,
        .file_name_length = file_name.length,
        .payload_length = file_stat.st_size,
    };

    int sent = 0, ret;

    if ((ret = send(s->sock_fd, &upload_header, sizeof upload_header, 0)) < 0) {
        log(LOG_ERROR, "Error sending header: %s\n", strerror(s->last_error));
        return -1;
    }
    sent += ret;

    if ((ret = send(s->sock_fd, file_name.data, upload_header.file_name_length, 0)) < 0) {
        log(LOG_ERROR, "Error sending file: %s\n", strerror(errno));
        return -1;
    }
    sent += ret;

    const int f = open(file_path, O_RDONLY);
    if ((ret = sendfile(s->sock_fd, f, NULL, upload_header.payload_length)) < 0) {
        log(LOG_ERROR, "Error sending file: %s\n", strerror(s->last_error));
        return -1;
    }
    close(f);

    sent += ret;
    return sent;
}

int client_main() {
    var server_endpoint = ipv4_endpoint_new(ip, port);
    var sock = socket_new(AF_INET);
    if (!tcp_client_connect(&sock, &server_endpoint)) {
        log(LOG_ERROR, "%s", strerror(sock.last_error));
        return -1;
    }
    dripbox_login(&sock, username);

    const char sync_dir[] = "./sync_dir/";
    struct stat dir_stat = {};
    if (stat(sync_dir, &dir_stat) < 0) {
        mkdir(sync_dir, S_IRWXU | S_IRWXG | S_IRWXO);
    }

    static char buffer[1 << 20] = {};
    static const struct string_view_t cmd_upload = (struct string_view_t){
        .data = "upload",
        .length = sizeof "upload" - 1,
    };

    while (true) {
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
            dripbox_upload(&sock, file_path.data);
        }
    }

    close(sock.sock_fd);
    return 0;
}
