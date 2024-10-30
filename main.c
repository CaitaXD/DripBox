#include <Allocator.h>
#include <stdio.h>
#include <common.h>
#include <Monitor.h>
#include <Network.h>
#include <stdlib.h>
#include <string.h>
#include <hash_set.h>

#include "dripbox_common.h"
#include "server.c"

enum {
    SUCCESS,
    USAGE_COMMAND,
};

const char *USAGE_MSG =
        "Usage: dripbox <client username | server> [ip] [port] [OPTIONS]\n"
        "\n"
        "Arguments:\n"
        "  client|server\t\tClient or server mode\n"
        "  username\t\t\tUsername to use\n"
        "  ip\t\t\tip address to bind to (default: 0.0.0.0)\n"
        "  port\t\t\tport to bind to (default: 25565)\n"
        "\n"
        "Options:\n"
        "  -h | --help\t\tShow this help message\n";

int parse_commandline(const int argc, char *argv[argc]) {
    if (argc < 2) {
        return USAGE_COMMAND;
    }
    int last_parsed = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            return USAGE_COMMAND;
        }
        switch (last_parsed++) {
            case 0:
                mode = argv[i];
                if (strncmp(mode, "client", strlen("client")) == 0) {
                    mode_type = MODE_CLIENT;
                    username.data = argv[i] + sizeof "client";
                    username.length = strnlen(username.data, 255);
                } else if (strcmp(mode, "server") == 0) {
                    mode_type = MODE_SERVER;
                } else {
                    mode_type = MODE_INVALID;
                    printf("Invalid mode: %s\n", mode);
                    return USAGE_COMMAND;
                }
                break;
            case 1:
                ip = inet_addr(argv[i]);
                break;
            case 2:
                port = atoi(argv[i]);
                break;
            default:
                break;
        }
    }
    return SUCCESS;
}

int client_main() {
    struct socket_t client = socket_new(AF_INET);
    tcp_client_connect(&client, &ipv4_endpoint_new(ip, port));
    if (client.last_error != 0) {
        log(LOG_INFO, "%s\n", strerror(client.last_error));
        return 1;
    }

    log(LOG_INFO, "Connected to server %s\n", socket_address_to_cstr(client.addr, &default_allocator));

    const struct dripbox_payload_header_t header = {
        .version = 1,
        .type = MSG_LOGIN,
        .length = username.length,
    };
    int ret = 0;
    ret |= send(client.sock_fd, &header, sizeof header, 0);
    ret |= send(client.sock_fd, username.data, username.length, 0);
    if (ret < 0) {
        log(LOG_INFO, "%s\n", strerror(client.last_error));
        return 1;
    }
    close(client.sock_fd);
    return 0;
}

int main(const int argc, char *argv[argc]) {
    switch (parse_commandline(argc, argv)) {
        case USAGE_COMMAND:
            printf("%s", USAGE_MSG);
            return 0;
        case SUCCESS:
            printf("Starting DripBox %s\n", mode);
            printf("IP: %s\n", inet_ntoa(*(struct in_addr *) &ip));
            printf("Port: %d\n", port);
            if (mode_type == MODE_CLIENT) {
                return client_main();
            }
            if (mode_type == MODE_SERVER) {
                return server_main();
            }
        default: unreachable();
    }
}
