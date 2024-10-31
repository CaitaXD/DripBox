#include <Allocator.h>
#include <stdio.h>
#include <common.h>
#include <Network.h>
#include <string.h>
#include <dripbox_common.h>
#include<sys/sendfile.h>

static int connect_and_send(struct socket_address_t *addr, const struct dripbox_payload_header_t header,
                            uint8_t data[header.length]) {
    struct socket_t client = socket_new(AF_INET);
    tcp_client_connect(&client, addr);
    if (client.last_error != 0) {
        log(LOG_ERROR, "%s\n", strerror(client.last_error));
        return -1;
    }
    int sent = 0, ret;

    if ((ret = send(client.sock_fd, &header, sizeof header, 0)) < 0) {
        log(LOG_ERROR, "Error sending header: %s\n", strerror(client.last_error));
        return -1;
    }
    sent += ret;

    if ((ret = send(client.sock_fd, data, header.length, 0)) < 0) {
        log(LOG_ERROR, "Error sending data: %s\n", strerror(client.last_error));
        return -1;
    }
    sent += ret;

    close(client.sock_fd);
    return sent;
}

static int connect_and_send_file(struct socket_address_t *addr, const struct dripbox_payload_header_t header,
                                 const char *file_path) {
    struct socket_t client = socket_new(AF_INET);
    tcp_client_connect(&client, addr);
    if (client.last_error != 0) {
        log(LOG_ERROR, "%s\n", strerror(client.last_error));
        return -1;
    }
    int sent = 0, ret;

    if ((ret = send(client.sock_fd, &header, sizeof header, 0)) < 0) {
        log(LOG_ERROR, "Error sending header: %s\n", strerror(client.last_error));
        return -1;
    }
    sent += ret;

    if ((ret = sendfile(client.sock_fd, open(file_path, O_RDONLY), NULL, header.length)) < 0) {
        log(LOG_ERROR, "Error sending file: %s\n", strerror(client.last_error));
        return -1;
    }
    sent += ret;

    close(client.sock_fd);
    return sent;
}

int client_main() {
    var server_endpoint = ipv4_endpoint_new(ip, port);
    const struct dripbox_payload_header_t login_msg_header = {
        .version = 1,
        .type = MSG_LOGIN,
        .length = username.length,
    };
    connect_and_send(&server_endpoint, login_msg_header, (uint8_t *) username.data);

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
            const struct string_view_t file_path = sv_token(cmd, sv_from_cstr(" ")).components[1];
            if (file_path.data[file_path.length - 1] == '\n') {
                file_path.data[file_path.length - 1] = 0;
            }

            struct stat file_stat = {0};
            if (stat(file_path.data, &file_stat) < 0) {
                log(LOG_ERROR, "%s %s\n", strerror(errno), file_path.data);
                continue;
            }

            const struct dripbox_payload_header_t upload_msg_header = {
                .version = 1,
                .type = MSG_UPLOAD,
                .length = file_stat.st_size,
            };
            connect_and_send_file(&server_endpoint, upload_msg_header, file_path.data);
        }
    }
    return 0;
}
