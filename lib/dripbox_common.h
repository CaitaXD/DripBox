#ifndef DRIPBOX_COMMON_H
#define DRIPBOX_COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>

enum msg_type {\
    MSG_NOOP = 0,
    MSG_LOGIN = 1,
    MSG_UPLOAD = 2,
    MSG_DOWNLOAD = 3,
    MSG_DELETE = 4,
    MSG_LIST = 5,
    MSG_ERROR = 6,
};

struct dripbox_file_times_t {
    char name[256]; //file name
    time_t ctime;
    time_t atime;
    time_t mtime;
} __attribute__((packed));

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

static uint8_t dripbox_checksum(const uint8_t *ptr, size_t sz);

static uint8_t dripbox_file_checksum(const char *path);

typedef int errno_t;

static errno_t socket_redirect_to_file(struct socket_t *s, const char *path, size_t length) {
    errno_t retval = 0;
    scope(FILE *file = fopen(path, "wb"), file && fclose(file)) {
        if (file == NULL) {
            retval = errno;
            continue;
        }

        enum { BUFFER_SIZE = 256 };
        uint8_t buffer[BUFFER_SIZE] = {};

        while (length > 0) {
            const ssize_t got = recv(s->sock_fd, buffer, min(length, BUFFER_SIZE), 0);
            if (got == 0) { break; }
            if (got < 0) {
                retval = s->error = errno;
                break;
            }
            length -= got;
            if (fwrite(buffer, sizeof(uint8_t), got, file) < 0) {
                retval = errno;
                break;
            }
        }
    }
    return retval;
}

static const char *msg_type_cstr(const enum msg_type msg_type) {
    switch (msg_type) {
    case MSG_UPLOAD: return "Upload";
    case MSG_DOWNLOAD: return "Download";
    case MSG_LOGIN: return "Login";
    case MSG_NOOP: return "NOOP";
    case MSG_DELETE: return "Delete";
    case MSG_LIST: return "List";
    default: return "INVALID MESSAGE";
    }
    unreachable();
}

uint8_t dripbox_checksum(const uint8_t *ptr, size_t sz) {
    uint8_t chk = 0;
    while (sz-- != 0)
        chk -= *ptr++;
    return chk;
}

static uint8_t dripbox_file_checksum(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }
    struct stat st = {};
    if (fstat(fileno(file), &st) < 0) {
        fclose(file);
        return -1;
    }
    size_t length = st.st_size;

    uint8_t chk = 0;
    uint8_t buffer[1024] = {};
    while (length > 0) {
        const ssize_t got = fread(buffer, sizeof(uint8_t), sizeof buffer, file);
        chk -= dripbox_checksum(buffer, got);
        length -= got;
    }
    fclose(file);
    return chk;
}

#endif //DRIPBOX_COMMON_H
