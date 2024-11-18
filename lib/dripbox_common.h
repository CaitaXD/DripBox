#ifndef DRIPBOX_COMMON_H
#define DRIPBOX_COMMON_H

#include <stdio.h>
#include <stdint.h>

enum msg_type {\
    MSG_NOOP = 0,
    MSG_LOGIN = 1,
    MSG_UPLOAD = 2,
    MSG_DOWNLOAD = 3,
    MSG_DELETE = 4,
    MSG_ERROR = 5,
};

enum { DRIPBOX_MAX_HEADER_SIZE = 4096 };

struct dripbox_msg_header_t {
    uint8_t version;
    uint8_t type;
} __attribute__((packed));

struct dripbox_login_header_t {
    size_t length;
} __attribute__((packed));

struct dripbox_upload_header_t {
    size_t file_name_length;
    size_t payload_length;
} __attribute__((packed));

struct dripbox_download_header_t {
    size_t file_name_length;
} __attribute__((packed));

struct dripbox_delete_header_t {
    size_t file_name_length;
} __attribute__((packed));

struct dripbox_error_header_t {
    size_t error_length;
} __attribute__((packed));

int32_t ip = INADDR_ANY;
uint16_t port = 25565;
char *mode = NULL;

enum { MODE_INVALID, MODE_CLIENT, MODE_SERVER } mode_type = MODE_INVALID;

typedef int errno_t;

static errno_t socket_read_to_file(const struct socket_t *s, const char *path, size_t length) {
    errno_t ret = 0;
    scope(FILE *file = fopen(path, "wb"), fclose(file)) {
        if (file == NULL) {
            ret = errno;
            break;
        }
        enum { BUFFER_SIZE = 256 };
        uint8_t buffer[BUFFER_SIZE] = {};

        while (length > 0) {
            const ssize_t got = recv(s->sock_fd, buffer, BUFFER_SIZE, 0);
            if (got == 0) { break; }
            if (got < 0) {
                ret = errno;
                break;
            }
            length -= got;
            if (fwrite(buffer, sizeof(uint8_t), got, file) < 0) {
                ret = errno;
                break;
            }
        }
    }
    return ret;
}

static const char *msg_type_cstr(const enum msg_type msg_type) {
    const char *str = NULL;
    switch (msg_type) {
    case MSG_UPLOAD:
        str = "Upload";
        break;
    case MSG_DOWNLOAD:
        str = "Download";
        break;
    case MSG_LOGIN:
        str = "Login";
        break;
    case MSG_NOOP:
        str = "NOOP";
        break;
    case MSG_DELETE:
        str = "Delete";
        break;
    default:
        str = "INVALID MESSAGE";
        break;
    }
    return str;
}

#endif //DRIPBOX_COMMON_H
