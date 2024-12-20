#include <stdio.h>
#include <common.h>
#include <dripbox_common.h>
#include <Monitor.h>
#include <Network.h>
#include <string.h>
#include <sys/types.h>
#include <coroutine.h>

static bool dripbox_dns_quit = false;
static struct server_id {
    uint32_t in_addr;
    struct uuidv7 uuid;
} main_server = {};

struct in_adress_port {
    in_addr_t in_addr;
    in_port_t port;
};

dynamic_array(struct in_adress_port) enqueued_dns_requests = NULL;
static struct monitor m_dns = MONITOR_INITIALIZER;

typedef packed_tuple(struct dripbox_msg_header, struct dripbox_in_adress_header) in_addr_msg_t;

void schedule_dns_request(struct socket sock, const struct socket_address remote) {
    using_conditional_monitor(&m_dns, main_server.uuid.integer == 0) {
        diagf(LOG_INFO, "Enqueued dns request to %s\n", socket_address_to_cstr(&remote, &mallocator));
        dynamic_array_push(&enqueued_dns_requests, ((struct in_adress_port) {
            .in_addr = socket_address_get_in_addr(&remote),
            .port = socket_address_get_port(&remote),
        }));
        monitor_return(&m_dns);
    }

    socket_write_struct_to(&sock, ((in_addr_msg_t) {
        { .version = 1, .type = DRIP_MSG_IN_ADRESS },
        { .in_addr = main_server.in_addr },
    }), &remote, 0);

    using_allocator_temp_arena {
        const var a = &allocator_temp_arena()->allocator;
        diagf(LOG_INFO, "Sent main server address %s to %s\n",
            in_adrr_to_cstr(main_server.in_addr, a),
            socket_address_to_cstr(&remote, a)
        );
    }
}

void *dispatch_dns_requests_worker(void *arg) {
    struct socket *sock = arg;
    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (!dripbox_dns_quit) {
        using_conditional_monitor(&m_dns, main_server.uuid.integer != 0) {
            while (dynamic_array_length(enqueued_dns_requests) > 0) {
                const struct in_adress_port in_addr_port = dynamic_array_pop(&enqueued_dns_requests);
                struct socket_address remote = ipv4_endpoint(ntohl(in_addr_port.in_addr), ntohs(in_addr_port.port));
                socket_write_struct_to(sock, ((in_addr_msg_t) {
                    { .version = 1, .type = DRIP_MSG_IN_ADRESS },
                    { .in_addr = main_server.in_addr },
                }), &remote, 0);

                using_allocator_temp_arena {
                    const var a = &allocator_temp_arena()->allocator;
                    diagf(LOG_INFO, "Sent main server address %s to %s\n",
                        in_adrr_to_cstr(main_server.in_addr, a),
                        in_adrr_to_cstr(in_addr_port.in_addr, a)
                    );
                }
            }
        }
    }
    return NULL;
}

int dns_main() {
    enqueued_dns_requests = dynamic_array_new(struct in_adress_port, &mallocator);
    struct socket sock = socket_new();
    socket_open(&sock, AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    socket_reuse_address(&sock, true);
    socket_bind(&sock, &ipv4_endpoint(INADDR_ANY, DRIPBOX_DNS_PORT));

    pthread_t dispatch_dns_requests_worker_id;
    pthread_create(&dispatch_dns_requests_worker_id, NULL, (void *) dispatch_dns_requests_worker, &sock);

    struct socket_address remote = ipv4_endpoint(0, 0);
    while (!dripbox_dns_quit) {
        if (!socket_pending_read(&sock, 0)) continue;

        uint8_t buffer[4096] = {};
        size_t length = 0;
        socket_read_from(&sock, size_and_address(buffer), 0, &remote);

        var dns_header = (struct dripbox_msg_header*)buffer;
        length += sizeof *dns_header;
        if (zero_initialized(*dns_header)) continue;
        if (!dripbox_expect_version(&sock, dns_header->version, 1)) continue;
        diagf(LOG_INFO, "Received message %s\n", msg_type_cstr(dns_header->type));

        switch (dns_header->type) {
        case DRIP_MSG_ADD_REPLICA: {
            const var add_replica_header = (struct dripbox_add_replica_header*)buffer;
            length += sizeof *add_replica_header;
            const struct uuidv7 server_uuid = add_replica_header->server_uuid;
            printf("Server UUID: %s\n", uuidv7_to_string(server_uuid.as_uuid).data);
            if (main_server.uuid.integer == 0 || election_higher_id(server_uuid, main_server.uuid)) {
                main_server.uuid = server_uuid;
                main_server.in_addr = socket_address_get_in_addr(&remote);
                diagf(LOG_INFO, "New main server: %s\n", uuidv7_to_string(main_server.uuid.as_uuid).data);
            }
            schedule_dns_request(sock, remote);
        } break;
        case DRIP_MSG_COORDINATOR: {
            const var coordinator_header = (struct dripbox_coordinator_header*)buffer;
            length += sizeof *coordinator_header;
            main_server.uuid = coordinator_header->coordinator_uuid;
            main_server.in_addr = socket_address_get_in_addr(&remote);
        } break;
        case DRIP_MSG_ELECTION: {
            main_server = zero(struct server_id);
        } break;
        case DRIP_MSG_DNS: {
            schedule_dns_request(sock, remote);
        } break;
        default: {
            ediag("Illegal message");
            diagf(LOG_INFO, "Type: 0X%X\n", dns_header->type);
        } break;
        }
    }

    pthread_join(dispatch_dns_requests_worker_id, NULL);
    return 0;
}
