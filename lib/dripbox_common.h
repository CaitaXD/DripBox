#ifndef DRIPBOX_COMMON_H
#define DRIPBOX_COMMON_H

#include <stdio.h>

enum msg_type {\
    MSG_NOOP = 0,
    MSG_LOGIN = 1,
    MSG_UPLOAD = 2,
};

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

struct string_view_t {
    size_t length;
    char *data;
};

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

#endif //DRIPBOX_COMMON_H
