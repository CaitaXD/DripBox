#ifndef DRIPBOX_COMMON_H
#define DRIPBOX_COMMON_H

#include <stdio.h>

enum msg_type {\
    MSG_NOOP = 0,
    MSG_LOGIN = 1,
};

struct dripbox_payload_header_t {
    unsigned char version: 1;
    unsigned char type: 7;
    unsigned char length: 8;
};

struct string_view_t {
    size_t length;
    char *data;
};

int32_t ip = INADDR_ANY;
uint16_t port = 25565;
char *mode = NULL;
struct string_view_t username = {};

enum { MODE_INVALID, MODE_CLIENT, MODE_SERVER } mode_type = MODE_INVALID;

#endif //DRIPBOX_COMMON_H
