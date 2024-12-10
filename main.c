#include <stdio.h>
#include <common.h>
#include <Network.h>
#include <stdlib.h>
#include <string.h>
#include <array.h>
#include "dripbox_common.h"
#include "server.c"
#include "client.c"
#include <ifaddrs.h>

enum {
    SUCCESS,
    USAGE_COMMAND,
};

const char *USAGE_MSG =
    "Usage: dripbox <client dripbox_username | server> [ip] [port] [OPTIONS]\n"
    "\n"
    "Arguments:\n"
    "  client|server\t\tClient or server mode\n"
    "  dripbox_username\t\t\tUsername to use\n"
    "  ip\t\t\tip address to bind to (default: 0.0.0.0)\n"
    "  port\t\t\tport to bind to (default: 25565)\n"
    "\n"
    "Options:\n"
    "  -h | --help\t\tShow this help message\n";

int foo(int a, int b);
int len(char *a);
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
                dripbox_username.data = argv[i] + sizeof "client";
                dripbox_username.length = strnlen(dripbox_username.data, 255);
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

void print_network_info(const struct ifaddrs *addrs) {
    const struct ifaddrs *tmp = addrs;
    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
            const struct sockaddr_in *pAddr = (struct sockaddr_in *) tmp->ifa_addr;
            printf("%s: %s\n", tmp->ifa_name, inet_ntoa(pAddr->sin_addr));
        }

        tmp = tmp->ifa_next;
    }
}
int main(const int argc, char *argv[argc]) {
    for (int i = 0; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");

    switch (parse_commandline(argc, argv)) {
    case USAGE_COMMAND:
        printf("%s", USAGE_MSG);
        return 0;
    case SUCCESS:
        printf("Starting DripBox %s\n", mode);
        printf("Ip=%s\n", inet_ntoa(*(struct in_addr *) &ip));
        printf("Port: %d\n", port);

        struct ifaddrs *addrs;
        getifaddrs(&addrs);
        print_network_info(addrs);
        int ret = -1;
        if (mode_type == MODE_CLIENT) {
            ret = client_main();
        }
        if (mode_type == MODE_SERVER) {
            ret = server_main(addrs);
        }
        freeifaddrs(addrs);
        return ret;
    default: unreachable();
    }
}
