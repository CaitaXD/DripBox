#ifndef DRIPBOX_COMMON_H
#define DRIPBOX_COMMON_H

#include <stdio.h>
#include <linux/limits.h>
#include <sys/sendfile.h>
#include <sys/stat.h>

enum msg_type {\
    MSG_NOOP = 0,
    MSG_LOGIN = 1,
    MSG_UPLOAD = 2,
    MSG_DOWNLOAD = 3,
    MSG_ERROR = 4,
};

enum { DRIPBOX_MAX_HEADER_SIZE = 4096 };

struct dripbox_generic_header_t {
    unsigned char version: 1;
    unsigned char type: 7;
} __attribute__((packed));

struct dripbox_login_header_t {
    unsigned char version: 1;
    unsigned char type: 7;
    size_t length;
} __attribute__((packed));

struct dripbox_upload_header_t {
    unsigned char version: 1;
    unsigned char type: 7;
    size_t file_name_length;
    size_t payload_length;
} __attribute__((packed));

struct dripbox_download_header_t {
    unsigned char version: 1;
    unsigned char type: 7;
    size_t file_name_length;
} __attribute__((packed));

struct dripbox_error_header_t {
    unsigned char version: 1;
    unsigned char type: 7;
    size_t error_length;
} __attribute__((packed));

struct string_view_t {
    size_t length;
    char *data;
};

struct readonly_span_t {
    size_t length;
    const uint8_t *data;
};

_Thread_local static char tls_dripbox_path_buffer[PATH_MAX] = {};

static struct string_view_t sv_skip(const struct string_view_t string, const size_t length) {
    return (struct string_view_t){
        .data = string.data + length,
        .length = string.length - length,
    };
}

static struct string_view_t sv_take(const struct string_view_t string, const size_t length) {
    return (struct string_view_t){
        .data = string.data,
        .length = length,
    };
}

static struct string_view_t sv_substr(const struct string_view_t string, const size_t offset, const size_t length) {
    return (struct string_view_t){
        .data = string.data + offset,
        .length = length,
    };
}

static ssize_t _sv_path_combine_impl(struct string_view_t *dst,
                                     const struct string_view_t a, const struct string_view_t b) {
    static const char PATH_SEPARATOR = '/';

    memcpy(dst->data, a.data, a.length);
    dst->length = a.length;
    if (a.length > 0 && a.data[a.length - 1] != PATH_SEPARATOR) {
        dst->data[a.length] = PATH_SEPARATOR;
        dst->length += 1;
    }

    memcpy(dst->data + dst->length, b.data, b.length);
    dst->length += b.length;

    if (dst->length > PATH_MAX) {
        dst->length = PATH_MAX;
    }
    return dst->length;
}

#define sv_path_combine(...) ({\
    struct string_view_t _buffer = {\
        .data = (char[PATH_MAX]){},\
        .length = 0,\
    };\
    const struct string_view_t __sv_args[] = {__VA_ARGS__};\
    const size_t __len = sizeof __sv_args / sizeof __sv_args[0];\
    struct string_view_t __acc = _buffer;\
    for (size_t i = 0; i < __len; i++) {\
        _sv_path_combine_impl(&__acc, __acc, __sv_args[i]);\
    }\
    __acc;\
})

#define path_combine(...) ({\
    char _buffer[PATH_MAX] = {};\
    char *__cstr_args[] = {__VA_ARGS__};\
    const size_t __len = sizeof __cstr_args / sizeof __cstr_args[0];\
    struct string_view_t __acc = (struct string_view_t){\
        .data = _buffer,\
        .length = 0,\
    };\
    for (size_t i = 0; i < __len; i++) {\
        _sv_path_combine_impl(&__acc, __acc, sv_from_cstr(__cstr_args[i]));\
    }\
    __acc.data[__acc.length] = 0;\
    __acc.data;\
})

static ssize_t sv_index(const struct string_view_t string, const struct string_view_t needle) {
    if (string.length < needle.length) {
        return -1;
    }
    const char *p = strstr(string.data, needle.data);
    if (p == NULL) {
        return -1;
    }
    const size_t index = p - string.data;
    if (index > string.length) {
        return -1;
    }
    return index;
}

#define ARRAY(N, T) struct { \
    T data[N];\
}

struct sv_pair {
    ARRAY(2, struct string_view_t);
};

static struct sv_pair sv_token(const struct string_view_t string,
                               const struct string_view_t delim) {
    struct sv_pair pair = {};
    const ssize_t index = sv_index(string, delim);
    if (index == -1) {
        pair.data[0] = string;
        pair.data[1] = (struct string_view_t){};
    }
    pair.data[0] = sv_take(string, index);
    pair.data[1] = sv_skip(string, index + 1);
    return pair;
}

static struct string_view_t sv_from_cstr(char *data) {
    return (struct string_view_t){
        .data = data,
        .length = strlen(data),
    };
}

static char *sv_to_cstr(const struct string_view_t sv, const struct allocator_t *allocator) {
    char *buffer = alloc(allocator, sv.length + 1);
    memcpy(buffer, sv.data, sv.length);
    buffer[sv.length] = 0;
    return buffer;
}

static bool sv_split_next(struct string_view_t *sv, const struct string_view_t delim, struct string_view_t *out_part) {
    const ssize_t index = sv_index(*sv, delim);
    if (index == -1) {
        *out_part = *sv;
        return false;
    }
    *out_part = sv_take(*sv, index);
    *sv = sv_skip(*sv, index + 1);
    return true;
}

int32_t ip = INADDR_ANY;
uint16_t port = 25565;
char *mode = NULL;

enum { MODE_INVALID, MODE_CLIENT, MODE_SERVER } mode_type = MODE_INVALID;

typedef int errno_t;

static errno_t sockdump_to_file(const struct socket_t *s, const char *path, size_t length) {
    errno_t ret = 0;
    scope(FILE *file = fopen(path, "wb"), fclose(file)) {
        if (file == NULL) {
            ret = errno;
            exit_scope;
        }
        enum { BUFFER_SIZE = 256 };
        uint8_t buffer[BUFFER_SIZE] = {};

        while (length > 0) {
            const ssize_t got = recv(s->sock_fd, buffer, BUFFER_SIZE, 0);
            if (got == 0) { exit_scope; }
            if (got < 0) {
                ret = errno;
                exit_scope;
            }
            length -= got;
            if (fwrite(buffer, sizeof(uint8_t), got, file) < 0) {
                ret = errno;
                exit_scope;
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
    default:
        str = "INVALID MESSAGE";
        break;
    }
    return str;
}

#endif //DRIPBOX_COMMON_H
