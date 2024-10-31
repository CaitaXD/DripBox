#ifndef DRIPBOX_COMMON_H
#define DRIPBOX_COMMON_H

#include <stdio.h>

enum msg_type {\
    MSG_NOOP = 0,
    MSG_LOGIN = 1,
    MSG_UPLOAD = 2,
};

struct dripbox_payload_header_t {
    unsigned char version: 1;
    unsigned char type: 7;
    size_t length;
};

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

#define VEC(N, T) { \
    T components[N];\
}

static struct sv_pair VEC(2, struct string_view_t) sv_token(const struct string_view_t string,
                                                            const struct string_view_t delim) {
    struct sv_pair pair = {};
    const ssize_t index = sv_index(string, delim);
    if (index == -1) {
        pair.components[0] = string;
        pair.components[1] = (struct string_view_t){};
    }
    pair.components[0] = sv_take(string, index);
    pair.components[1] = sv_skip(string, index + 1);
    return pair;
}

static struct string_view_t sv_from_cstr(char *data) {
    return (struct string_view_t){
        .data = data,
        .length = strlen(data),
    };
}

int32_t ip = INADDR_ANY;
uint16_t port = 25565;
char *mode = NULL;
struct string_view_t username = {};

enum { MODE_INVALID, MODE_CLIENT, MODE_SERVER } mode_type = MODE_INVALID;

#endif //DRIPBOX_COMMON_H
