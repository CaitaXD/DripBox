#include <stdio.h>
#include <dirent.h>
#include <common.h>
#include <dripbox_common.h>
#include <Monitor.h>
#include <Network.h>
#include <string.h>
#include <hash_set.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "string_view.h"
#include <ifaddrs.h>
#include <coroutine.h>

static bool dripbox_server_quit = false;

#define DRIPBOX_REPLICAS 2

struct user {
    struct string_view username;
    struct socket socket;
};

struct replica {
    struct uuidv7 uuid;
    struct socket socket;
};

typedef hash_table(char *, struct user) UsersHashTable;
typedef key_value_pair(char *, struct user) UserKVP;
typedef hash_table(char*, struct replica) ReplicaHashTable;
typedef key_value_pair(char*, struct replica) ReplicaKVP;

// TODO: Finish migrating this
struct dripbox_server_context {
    struct tcp_listener *listener;
    UsersHashTable *hash_table;
};
// TODO: To this
struct dripbox_server {
    struct socket dns_socket;
    in_addr_t dns_in_addr;
    struct ifaddrs interface_address;
    struct tcp_listener listener;
    UsersHashTable ht_users;
    ReplicaHashTable ht_replicas;
    struct monitor m_replicas;
    struct monitor m_users;
    struct uuidv7 uuid;
    struct queue co_scheduler;
    pthread_t incoming_connections_worker;
    pthread_t network_worker;
    packed_tuple(struct dripbox_msg_header, struct dripbox_add_replica_header) discovery_probe;
    struct z_string userdata_dir;
    enum server_role {
        MASTER_ROLE,
        REPLICA_ROLE
    } server_role;
    enum election_state election_state;
    bool quit;
    struct coroutine election_coroutine_handle;
    struct uuidv7 coordinator_uuid;
} dripbox_server;

static bool election_higher_id(const struct uuidv7 a, const struct uuidv7 b) {
    if (a.timestamp == b.timestamp) {
        return a.integer < b.integer;
    }
    return a.timestamp < b.timestamp;
}

static void dripbox_server_start_election(struct dripbox_server *dripbox_server);

static void* dripbox_server_election_coroutine(struct coroutine *co, struct dripbox_server *dripbox_server);

static void dripbox_server_handle_client_login(struct dripbox_server *dripbox_server, struct socket *client);

static void dripbox_server_handle_client_upload(struct dripbox_server *dripbox_server, struct user *user);

static void dripbox_server_handle_client_download(struct user *user);

static void dripbox_server_handle_client_delete(struct dripbox_server *dripbox_server, struct user *user);

static void dripbox_server_handle_client_list(struct user *user);

static void dripbox_server_handle_client_massage(struct dripbox_server *dripbox_server, struct user *user);

static void *dripbox_server_incoming_connections_worker(const void *arg);

static void *dripbox_server_network_worker(void *arg);

static void dripbox_server_handle_add_replica(struct dripbox_server *dripbox_server, struct socket *sock);

static void dripbox_server_handle_replica_massage(struct dripbox_server *dripbox_server, struct replica *replica);

static void dripbox_handle_replica_upload(struct dripbox_server *dripbox_server, struct socket *sock);

static void dripbox_server_upload_user_to_replica(const struct dripbox_server *dripbox_server, struct replica *replica);

static void dripbox_server_handle_list_user(const struct dripbox_server *dripbox_server, struct socket *sock);

static int dripbox_download_replica_file(const struct dripbox_server *dripbox_server, struct socket *sock, struct z_string filename);

static void dripbox_server_handle_replica_download(struct dripbox_server *dripbox_server, struct socket *sock);

static ssize_t dripbox_upload_client_file(
    struct socket *sock,
    struct string_view file_name,
    uint8_t checksum,
    struct z_string path
);

static ssize_t dripbox_upload_replica_file(
    struct socket *sock,
    uint8_t checksum,
    struct z_string path
);

static void dripbox_delete_client_file(struct user *user, struct string_view file_name);

static void* server_discover_replicas(struct coroutine *co) {
    struct ifaddrs server_addr;
    struct socket multicast_sock;
    struct socket_address multicast_addr;
    struct socket_address remote;

    co_on_init(co) {
        multicast_sock = socket_new();
        struct sockaddr_in addrin = sockaddr_in(DRIPBOX_REPLICA_MULTICAST_GROUP, DRIPBOX_REPLICA_PORT);
        multicast_addr = (struct socket_address) {
            .sa = (void*)new(&mallocator, struct sockaddr_in, addrin),
            .addr_len = sizeof(struct sockaddr_in),
        };

        remote = (struct socket_address) {
            .sa = (void*)new(&mallocator, struct sockaddr_in, zero(struct sockaddr_in)),
            .addr_len = sizeof(struct sockaddr_in),
        };
    }

    COROUTINE(co, server_discover_replicas,
              multicast_addr, multicast_sock,
              remote, server_addr) {
        socket_open(&multicast_sock, AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        socket_reuse_address(&multicast_sock, true);
        socket_bind(&multicast_sock, &ipv4_endpoint(INADDR_ANY, DRIPBOX_REPLICA_PORT));
        socket_blocking(&multicast_sock, false);
        socket_join_multicast_group(&multicast_sock, (struct ip_mreq) {
            .imr_multiaddr = htonl(DRIPBOX_REPLICA_MULTICAST_GROUP),
            .imr_interface = INADDR_ANY,
        });
        socket_option(&multicast_sock, IPPROTO_IP, IP_MULTICAST_LOOP, false);

        socket_write_struct_to(&multicast_sock, dripbox_server.discovery_probe, &multicast_addr, 0);
        while (!dripbox_server_quit) {
            if(!socket_pending_read(&multicast_sock, 0)) {
                co_yield(co);
                continue;
            }
            typedef typeof(dripbox_server.discovery_probe) discovery_probe_t;
            var discovery_response = socket_read_struct_from(&multicast_sock, discovery_probe_t, &remote, 0);
            if(!dripbox_expect_version(&multicast_sock, discovery_response.item1.version, 1)) continue;
            if (discovery_response.item1.type == DRIP_MSG_LOGIN) {
                socket_write_struct_to(&multicast_sock, dripbox_server.discovery_probe, &multicast_addr, 0);
                continue;
            }

            using_monitor(&dripbox_server.m_replicas) {
                var a = hash_table_allocator(dripbox_server.ht_replicas);
                char *remote_ip = ipv4_cstr(&remote, a);
                struct replica replica = {
                    .uuid = discovery_response.item2.server_uuid,
                    .socket = (struct socket) {
                        .sock_fd = -1,
                        .addr = allocator_copy(a, &remote, sizeof remote),
                    },
                };
                if (hash_table_insert(&dripbox_server.ht_replicas, remote_ip, replica)) {
                    diagf(LOG_INFO, "Recived replica: Id=%s remote=%s\n",
                        uuidv7_to_string(discovery_response.item2.server_uuid.as_uuid).data,
                        remote_ip
                    );
                }
                else {
                    diagf(LOG_INFO, "Recived replica: Id=%s remote=%s (already exists)\n",
                        uuidv7_to_string(discovery_response.item2.server_uuid.as_uuid).data,
                        remote_ip
                    );
                    allocator_dealloc(a, remote_ip);
                }
            }
        }
    }
    allocator_dealloc(&mallocator, multicast_addr.sa);
    allocator_dealloc(&mallocator, remote.sa);
    return NULL;
}

static void *dripbox_server_connect_replica_async(struct coroutine *co,
                                                  struct socket *replica_sock,
                                                  struct socket_address *remote) {
    COROUTINE(co, dripbox_server_connect_replica_async, replica_sock, remote) {
        bool connected = socket_connect(replica_sock, remote);
        while (replica_sock->error.code == EAGAIN) {
            co_yield(co);
            replica_sock->error.code = 0;
            connected = socket_connect(replica_sock, remote);
        }
        socket_write_struct(replica_sock, dripbox_server.discovery_probe, 0);

        if (replica_sock->error.code == 0) {
            diagf(LOG_INFO, "Connected to replica\n");
        }
        else {
            ediag("dripbox_server_connect_replica_async");
        }
        co_return(co, connected);
    }
    return NULL;
}

static void *dripbox_server_connect_replicas(struct coroutine *co) {
    COROUTINE(co, dripbox_server_connect_replicas) {
        while (!dripbox_server_quit) {
            using_monitor(&dripbox_server.m_replicas) {
                for (int i = 0; i < hash_set_capacity(dripbox_server.ht_replicas); i++) {
                    struct hash_entry_t *entry;
                    if (!hash_set_try_entry(dripbox_server.ht_replicas, i, &entry)) continue;

                    const var kvp = (ReplicaKVP*) entry->value;
                    struct replica *replica = &kvp->value;
                    struct socket *replica_sock = &replica->socket;
                    struct socket_address* remote = replica->socket.addr;

                    if (replica->socket.sock_fd == -1) {
                        socket_open(replica_sock, AF_INET, SOCK_STREAM, IPPROTO_TCP);
                        socket_blocking(replica_sock, false);
                        socket_reuse_address(replica_sock, true);
                        socket_adress_set_port(remote, port);
                        struct coroutine *co_connect_async = new(&mallocator, struct coroutine, zero(struct coroutine));
                        dripbox_server_connect_replica_async(co_connect_async, replica_sock, remote);
                        fifo_push(dripbox_server.co_scheduler, co_connect_async);
                    }
                }
            }
            co_yield(co);
        }
    }
    return NULL;
}
static int server_main(const struct ifaddrs *addrs) {
    signal(SIGPIPE, SIG_IGN);
    const struct uuidv7 server_uuid = uuidv7_new();
    printf("Server UUID: %s\n", uuidv7_to_string(server_uuid.as_uuid).data);
    printf("Server UUID Timestamp: %lu\n", (uint64_t)server_uuid.timestamp);
    struct ifaddrs server_addr = {};
    while (addrs) {
        // TODO: Hardcoded interface name, but i dont care right now and possibly will never care
        if (addrs->ifa_addr && addrs->ifa_addr->sa_family == AF_INET && sv_starts_with(addrs->ifa_name, "e")) {
            server_addr = *addrs;
            break;
        }
        addrs = addrs->ifa_next;
    }

    struct tcp_listener listener = tcp_listener_new();
    tcp_listener_bind(&listener, AF_INET, &ipv4_endpoint(INADDR_ANY, port));
    tcp_listener_listen(&listener, SOMAXCONN);
    if (listener.error.code != 0) {
        diagf(LOG_INFO, "%s\n", strerror(listener.error.code));
        return 1;
    }

    pthread_t incoming_connections_worker_id = 0, nextwork_worker_id = 0;
    var users_hash_table = hash_table_new(char*, struct user,
                                          string_hash, string_comparer_equals,
                                          &mallocator);
    struct dripbox_server_context ctx = {
        .listener = &listener,
        .hash_table = (UsersHashTable *) &users_hash_table,
    };

    void* scheduler_backlog = dynamic_array_new(struct coroutine*, &mallocator);

    struct socket dns_socket = socket_new();
    socket_open(&dns_socket, AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    socket_reuse_address(&dns_socket, true);
    struct socket_address dns_remote = ipv4_endpoint(0, 0);
    struct socket_address dns_addr = ipv4_endpoint(ntohl(dns_in_adrr), DRIPBOX_DNS_PORT);

    typedef packed_tuple(struct dripbox_msg_header, struct dripbox_add_replica_header) add_replica_msg_t;
    typedef packed_tuple(struct dripbox_msg_header, struct dripbox_in_adress_header) in_addr_msg_t;

    retry:
    diagf(LOG_INFO, "Sending add replica message to dns\n");
    socket_write_struct_to(&dns_socket, ((add_replica_msg_t) {
        { .version = 1, .type = DRIP_MSG_ADD_REPLICA },
        { .server_uuid = server_uuid },
    }), &dns_addr, 0);

    diagf(LOG_INFO, "Waiting for dns response\n");
    const var msg = socket_read_struct_from(&dns_socket, in_addr_msg_t, &dns_remote, 0);
    if (!dripbox_expect_version(&dns_socket, msg.item1.version, 1)) {
        diagf(LOG_INFO, "Received invalid version %d\n", msg.item1.version);
        goto retry;
    }

    if (!dripbox_expect_msg(&dns_socket, msg.item1.type, DRIP_MSG_IN_ADRESS)) {
        diagf(LOG_INFO, "Received invalid message type %d\n", msg.item1.type);
        goto retry;
    }

    using_allocator_temp_arena {
        const var a = &allocator_temp_arena()->allocator;
        diagf(LOG_INFO, "Received main server address %s from dns\n", in_adrr_to_cstr(msg.item2.in_addr, a));
    }

    dripbox_server = (struct dripbox_server) {
        .dns_socket = dns_socket,
        .dns_in_addr = msg.item2.in_addr,
        .interface_address = server_addr,
        .listener = listener,
        .ht_users = (void*)hash_table_new(char*, struct user, string_hash, string_comparer_equals, &mallocator),
        .ht_replicas = (void*)hash_table_new(char*, struct replica, string_hash, string_comparer_equals, &mallocator),
        .m_replicas = MONITOR_INITIALIZER,
        .m_users = MONITOR_INITIALIZER,
        .uuid = server_uuid,
        .coordinator_uuid = server_uuid,
        .co_scheduler = dynamic_array_fifo(&scheduler_backlog),
        .discovery_probe = {
            { .version = 1, .type = DRIP_MSG_ADD_REPLICA },
            { .server_uuid = server_uuid },
        },
        .userdata_dir = z_cstr("userdata/"),
        .server_role = MASTER_ROLE,
        .quit = false,
        .election_coroutine_handle = co_stack(126),
    };

    struct coroutine *co_add_replicas = &co_stack(4096);
    server_discover_replicas(co_add_replicas);
    fifo_push(dripbox_server.co_scheduler, co_add_replicas);

    struct coroutine *co_connect_replicas = &co_stack(4096);
    dripbox_server_connect_replicas(co_connect_replicas);
    fifo_push(dripbox_server.co_scheduler, co_connect_replicas);

    pthread_create(&incoming_connections_worker_id, NULL, (void *) dripbox_server_incoming_connections_worker, &ctx);
    pthread_create(&nextwork_worker_id, NULL, (void *) dripbox_server_network_worker, &dripbox_server);

    struct coroutine shcedureler = co_stack(128);
    while (!dripbox_server_quit) {
        co_queue_dispatch(&shcedureler, dripbox_server.co_scheduler);
    }

    pthread_join(incoming_connections_worker_id, NULL);
    pthread_join(nextwork_worker_id, NULL);

    return 0;
}

static void dripbox_server_handle_client_login(struct dripbox_server *dripbox_server, struct socket *client) {
    const var login_header = socket_read_struct(client, struct dripbox_login_header, 0);
    const struct string_view username = sv_new(
        login_header.length,
        socket_read_array(client, char, login_header.length, 0)
    );

    struct allocator *a = hash_set_allocator(dripbox_server->ht_users);
    const struct user _user = {
        .username = (struct string_view){
            .data = cstr_sv(username, a),
            .length = username.length,
        },
        .socket = *client,
    };

    const char *adress_cstr = ipv4_cstr(client->addr, a);
    hash_table_update(&dripbox_server->ht_users, adress_cstr, _user);

    const struct z_string dirpath = path_combine(dripbox_server->userdata_dir, username);
    if (stat(dripbox_server->userdata_dir.data, &(struct stat){}) < 0) {
        mkdir(dripbox_server->userdata_dir.data, S_IRWXU | S_IRWXG | S_IRWXO);
    }
    if (stat(dirpath.data, &(struct stat){}) < 0) {
        mkdir(dirpath.data, S_IRWXU | S_IRWXG | S_IRWXO);
    }
    
    for (int j = 0; j < hash_set_capacity(dripbox_server->ht_replicas); ++j)
    {
    	struct hash_entry_t *rentry;
	if (!hash_set_try_entry(dripbox_server->ht_replicas, i, &rentry)) continue; 
	
	var replica = (ReplicaKVP*)rentry->value;
	
        for (int i = 0; i < hash_set_capacity(dripbox_server->ht_replicas); ++i)
        {
    	    struct hash_entry_t *uentry;
            if (!hash_set_try_entry(dripbox_server->ht_users, i, &uentry)) continue;
            
            var user = (UserKVP*)uentry->value;
            
            typedef packed_tuple(struct dripbox_msg_header, struct dripbox_send_client_header) send_cli_msg;
            
            socket_write_struct(&replica->value.socket, ((send_cli_msg) {
                { 1, DRIP_MSG_SEND_CLIENT },
                { .client_username_len = user->value.username.lenght, .in_addr = socket_address_get_in_addr(user->value.socket.addr) }	
            }), 0);
            socket_write(&replica->value.socket, sv_deconstruct(user->value.username, 0);
       }
    }
}

ssize_t dripbox_upload_client_file(struct socket *sock,
                                   const struct string_view file_name,
                                   const uint8_t checksum,
                                   const struct z_string path) {
    struct stat st;
    if (stat(path.data, &st) < 0) {
        dripbox_send_error(sock, errno, SV(path));
        return 0;
    }

    if (S_ISDIR(st.st_mode)) {
        dripbox_send_error(sock, EISDIR, SV(path));
        return 0;
    }

    scope(FILE* file = fopen(path.data, "rb"), file && fclose(file)) {
        if (file == NULL) {
            dripbox_send_error(sock, errno, SV(path));
            continue;
        }

        socket_write_struct(sock, ((struct dripbox_msg_header) {
            .version = 1,
            .type = DRIP_MSG_UPLOAD,
        }), 0);

        socket_write_struct(sock, ((struct dripbox_upload_header) {
            .file_name_length = file_name.length,
            .payload_length = st.st_size,
        }), 0);

        socket_write(sock, sv_deconstruct(file_name), 0);
        socket_write_struct(sock, checksum, 0);
        socket_write_file(sock, file, st.st_size);
    }
    return st.st_size;
}

void dripbox_delete_client_file(struct user *user, const struct string_view file_name) {
    struct socket *client = &user->socket;

    socket_write_struct(client, ((struct dripbox_msg_header) {
        .version = 1,
        .type = DRIP_MSG_DELETE,
    }), 0);

    socket_write_struct(client, ((struct dripbox_delete_header) {
        .file_name_length = file_name.length,
    }), 0);

    socket_write(client, sv_deconstruct(file_name), 0);

    if (client->error.code != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(client->error.code));
    }
}

static void dripbox_server_handle_client_upload(struct dripbox_server *dripbox_server, struct user *user) {
    struct socket *client = &user->socket;
    const var upload_header = socket_read_struct(client, struct dripbox_upload_header, 0);
    const struct string_view file_name = sv_new(
        upload_header.file_name_length,
        socket_read_array(client, char, upload_header.file_name_length, 0)
    );
    const uint8_t client_checksum = socket_read_struct(client, uint8_t, 0);
    const struct string_view username = user->username;
    const struct z_string dir_path = path_combine(dripbox_server->userdata_dir, username);
    if (mkdir(dir_path.data, S_IRWXU | S_IRWXG | S_IRWXO) < 0 && errno != EEXIST) {
        ediagf("mkdir: "sv_fmt"\n", (int)sv_deconstruct(dir_path));
        return;
    }
    const struct z_string full_path = path_combine(dir_path, file_name);

    if (stat(full_path.data, &(struct stat){}) < 0) goto upload_file;
    if (dripbox_file_checksum(full_path.data) == client_checksum) {
        socket_redirect_to_file(client, "/dev/null", upload_header.payload_length);
        return;
    }
upload_file:
    socket_redirect_to_file(client, full_path.data, upload_header.payload_length);
    if (client->error.code != 0) {
        diagf(LOG_INFO, "%s\n", strerror(client->error.code));
        return;
    }

    const var users_header = hash_table_header(dripbox_server->ht_users);
    for (int i = 0; i < users_header->capacity; i++) {
        struct hash_entry_t *entry;
        if (!hash_set_try_entry(dripbox_server->ht_users, i, &entry)) continue;

        const var kvp = (UserKVP*) entry->value;
        if (!sv_equals(kvp->value.username, username)) { continue; }
        if (kvp->value.socket.sock_fd == user->socket.sock_fd) { continue; }
        if (kvp->value.socket.sock_fd == -1) { continue; }

        dripbox_upload_client_file(&kvp->value.socket, file_name, client_checksum, full_path);
        diagf(LOG_INFO, "Uploaded "sv_fmt" to "sv_fmt" Size=%ld Ip=%s Checksum: 0X%X\n",
              (int)sv_deconstruct(file_name),
              (int)sv_deconstruct(kvp->value.username),
              upload_header.payload_length,
              kvp->key,
              client_checksum
        );
    }

    for (int i = 0; i < hash_set_capacity(dripbox_server->ht_replicas); i++) {
        struct hash_entry_t *entry;
        if (!hash_set_try_entry(dripbox_server->ht_replicas, i, &entry)) continue;

        const var kvp = (ReplicaKVP*) entry->value;
        struct replica *replica = &kvp->value;
        if (replica->socket.sock_fd < 0) continue;
        dripbox_upload_replica_file(&replica->socket, client_checksum, full_path);
        if (replica->socket.error.code == 0) {
            diagf(LOG_INFO, "Replicated "sv_fmt" Checksum: 0X%X\n",
                (int)sv_deconstruct(file_name),
                client_checksum
            );
        }
    }
}

void dripbox_server_handle_client_download(struct user *user) {
    struct socket *client = &user->socket;

    const var download_header = socket_read_struct(client, struct dripbox_download_header, 0);
    const var file_name = sv_new(
        download_header.file_name_length,
        socket_read_array(client, char, download_header.file_name_length, 0)
    );

    if (client->error.code != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(client->error.code));
        return;
    }

    const struct string_view username = user->username;
    const struct z_string path = path_combine(dripbox_server.userdata_dir, username, file_name);

    struct stat st = {};
    if (stat(path.data, &st) < 0) {
        dripbox_send_error(client, errno, SV(path));
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        dripbox_send_error(client, EISDIR, SV(path));
        return;
    }

    diagf(LOG_INFO, "Uploading "sv_fmt" to "sv_fmt" Size=%ld",
        (int)sv_deconstruct(file_name),
        (int)sv_deconstruct(username),
        st.st_size
    );

    scope(FILE* file = fopen(path.data, "rb"), fclose(file)) {
        if (file == NULL) {
            dripbox_send_error(client, errno, SV(path));
            break;
        }

        socket_write_struct(client, ((struct dripbox_msg_header) {
            .version = 1,
            .type = DRIP_MSG_BYTES,
        }), 0);

        socket_write_struct(client, ((struct dripbox_bytes_header) {
            .length = st.st_size,
        }), 0);

        socket_write_file(client, file, st.st_size);
        if (client->error.code != 0) {
            diagf(LOG_ERROR, "%s\n", strerror(client->error.code));
        }
    }
}

void dripbox_server_handle_client_delete(struct dripbox_server *dripbox_server, struct user *user) {
    struct socket *client = &user->socket;
    const var delete_header = socket_read_struct(client, struct dripbox_delete_header, 0);
    const struct string_view file_name = sv_new(
        delete_header.file_name_length,
        socket_read_array(client, char, delete_header.file_name_length, 0)
    );
    const struct string_view username = user->username;
    const struct z_string path = path_combine(dripbox_server->userdata_dir, username, file_name);

    if (client->error.code != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(client->error.code));
        return;
    }

    if (stat(path.data, &(struct stat){}) < 0) {
        dripbox_send_error(client, errno, SV(path));
        return;
    }

    if (unlink(path.data) < 0) {
        diagf(LOG_ERROR, "%s\n", strerror(errno));
    } else {
        diagf(LOG_INFO, "Deleted "sv_fmt"\n", (int)sv_deconstruct(file_name));
    }

    for (int i = 0; i < hash_table_capacity(dripbox_server->ht_users); i++) {
        struct hash_entry_t *entry;
        if (!hash_set_try_entry(dripbox_server->ht_users, i, &entry)) continue;

        const var kvp = (UserKVP*) entry->value;
        if (!sv_equals(kvp->value.username, user->username)) { continue; }
        if (kvp->value.socket.sock_fd == user->socket.sock_fd) { continue; }
        if (kvp->value.socket.sock_fd == -1) { continue; }

        dripbox_delete_client_file(&kvp->value, file_name);

        diagf(LOG_INFO, "Deleting "sv_fmt" from "sv_fmt" Ip=%s\n",
              (int)sv_deconstruct(file_name),
              (int)sv_deconstruct(kvp->value.username),
              kvp->key
        );
    }
}

static void dripbox_server_handle_client_massage(struct dripbox_server *dripbox_server, struct user *user) {
    if (dripbox_server->uuid.integer != dripbox_server->coordinator_uuid.integer) {
        diagf(LOG_INFO, "TODO: Redirect client to coordinator\n");
    }
    struct socket *client = &user->socket;
    const var msg_header = socket_read_struct(client, struct dripbox_msg_header, 0);
    if (zero_initialized(msg_header)) return;
    if (!dripbox_expect_version(client, msg_header.version, 1)) return;
    const char *msg_type = msg_type_cstr(msg_header.type);
    diagf(LOG_INFO, "Recieved Message %s\n", msg_type);
    switch (msg_header.type) {
    case DRIP_MSG_NOOP: break;
    case DRIP_MSG_LOGIN: {
        ediag("Redundant login attempt by client process");
        break;
    }
    case DRIP_MSG_UPLOAD: {
        dripbox_server_handle_client_upload(dripbox_server, user);
        break;
    }
    case DRIP_MSG_DOWNLOAD: {
        dripbox_server_handle_client_download(user);
        break;
    }
    case DRIP_MSG_DELETE: {
        dripbox_server_handle_client_delete(dripbox_server, user);
        break;
    }
    case DRIP_MSG_LIST: {
        dripbox_server_handle_client_list(user);
        break;
    }
    case DRIP_MSG_LIST_USER:
    case DRIP_MSG_ELECTION:
    case DRIP_MSG_ADD_REPLICA: {
        ediag("Client process attempted to make a server operation");
        break;
    }
    default:
        diagf(LOG_INFO, "Type: 0X%X\n", msg_header.type);
        break;
    }
    if (client->error.code != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(client->error.code));
        client->error.code = 0;
    }
}

static ssize_t dripbox_upload_replica_file(struct socket *sock,
                                           const uint8_t checksum,
                                           struct z_string path) {
    struct stat st;
    const struct string_view svpath = SV(path);
    if (stat(path.data, &st) < 0) {
        dripbox_send_error(sock, errno, svpath);
        return 0;
    }

    if (S_ISDIR(st.st_mode)) {
        dripbox_send_error(sock, EISDIR, svpath);
        return 0;
    }

    scope(FILE* file = fopen(path.data, "rb"), file && fclose(file)) {
        if (file == NULL) {
            dripbox_send_error(sock, errno, svpath);
            break;
        }

        socket_write_struct(sock, ((struct dripbox_msg_header) {
            .version = 1,
            .type = DRIP_MSG_UPLOAD,
        }), 0);

        socket_write_struct(sock, ((struct dripbox_upload_header) {
            .file_name_length = svpath.length,
            .payload_length = st.st_size,
        }), 0);

        socket_write(sock, sv_deconstruct(svpath), 0);
        socket_write(sock, 1, "", 0);
        socket_write_struct(sock, checksum, 0);
        socket_write_file(sock, file, st.st_size);
    }
    return st.st_size;
}

static void dripbox_server_handle_replica_massage(struct dripbox_server *dripbox_server, struct replica *replica) {
    struct socket *sock = &replica->socket;
    const var msg_header = socket_read_struct(sock, struct dripbox_msg_header, 0);
    if (zero_initialized(msg_header)) return;
    if (!dripbox_expect_version(sock, msg_header.version, 1)) return;

    const char *msg_type = msg_type_cstr(msg_header.type);
    diagf(LOG_INFO, "Recieved Replica Message %s\n", msg_type);
    switch (msg_header.type) {
    case DRIP_MSG_NOOP: {
    } break;
    case DRIP_MSG_LIST: {
        dripbox_server_upload_user_to_replica(dripbox_server, replica);
    } break;
    case DRIP_MSG_LIST_USER: {
        dripbox_server_handle_list_user(dripbox_server, &replica->socket);
    } break;
    case DRIP_MSG_UPLOAD: {
        dripbox_handle_replica_upload(dripbox_server, sock);
    } break;
    case DRIP_MSG_ADD_REPLICA: {
        ediag("Redundant add replica attempt by server process");
    } break;
    case DRIP_MSG_SEND_CLIENT: {
        var msg = socket_read_struct(struct dripbox_send_client_header, 0);
        var in_addr = msg.in_addr;
        var username_len = msg.client_username_len;
        var username = sv_new(
          username_len
          socket_read_array(client, char, username_len, 0)
        );
    }
    case DRIP_MSG_COORDINATOR: {
        const var coordinator_header = socket_read_struct(sock, struct dripbox_coordinator_header, 0);
        const struct uuidv7 coordiantor_uuid = coordinator_header.coordinator_uuid;
        struct string36 uuid_str = uuidv7_to_string(coordiantor_uuid.as_uuid);
        if (election_higher_id(coordiantor_uuid, dripbox_server->uuid)) {
            dripbox_server->election_state = ELECTION_STATE_NONE;
            dripbox_server->server_role = REPLICA_ROLE;
            dripbox_server->coordinator_uuid = coordiantor_uuid;
            diagf(LOG_INFO, "Received coordinator message from replica %s\n", uuid_str.data);
        }
        else {
            diagf(LOG_INFO, "Coordinator %s is newer than us, starting election\n", uuid_str.data);
            dripbox_server->election_state = ELECTION_STATE_NONE;
            dripbox_server_start_election(dripbox_server);
        }
    } break;
    case DRIP_MSG_ELECTION: {
        dripbox_server_start_election(dripbox_server);
    } break;
    case DRIP_MSG_DOWNLOAD: {
        dripbox_server_handle_replica_download(dripbox_server, sock);
    } break;
    case DRIP_MSG_DELETE:
    case DRIP_MSG_LOGIN: {
        ediag("Server process attempted to make a client operation");
    } break;
    default: {
        diagf(LOG_INFO, "Type: 0X%X\n", msg_header.type);
    } break;
    }
    if (sock->error.code != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(sock->error.code));
        sock->error.code = 0;
    }
}

void *dripbox_server_incoming_connections_worker(const void *arg) {
    const struct dripbox_server_context *context = arg;
    struct tcp_listener *listener = context->listener;
    var addr = ipv4_endpoint(0, 0);

    socket_blocking(&listener->as_socket, false);
    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (!dripbox_server_quit) {
        struct socket incoming = {};
        while (tcp_server_incoming_next(listener, &incoming, &addr)) {
            const var msg_header = socket_read_struct(&incoming, struct dripbox_msg_header, 0);
            if (zero_initialized(msg_header)) continue;
            if (!dripbox_expect_version(&incoming, msg_header.version, 1)) continue;

            const char *msg_type = msg_type_cstr(msg_header.type);
            diagf(LOG_INFO, "Received Message %s\n", msg_type);
            switch (msg_header.type) {
            case DRIP_MSG_LOGIN: {
                using_monitor(&dripbox_server.m_users)
                    dripbox_server_handle_client_login(&dripbox_server, &incoming);
            } break;
            case DRIP_MSG_ADD_REPLICA: {
                using_monitor(&dripbox_server.m_replicas) {
                    dripbox_server_handle_add_replica(&dripbox_server, &incoming);
                    socket_write_struct(&incoming, ((struct dripbox_msg_header) { 1, DRIP_MSG_LIST }), 0);
                }
            } break;
            case DRIP_MSG_ELECTION: {
                using_monitor(&dripbox_server.m_replicas)
                using_monitor(&dripbox_server.m_users) {
                    dripbox_server_start_election(&dripbox_server);
                }
            } break;
            default:
                ediag("Unidentified host attempted to send a message before login");
                diagf(LOG_INFO, "Type: 0X%X\n", msg_header.type);
                break;
            }
        }
    }
    return NULL;
}

void *dripbox_server_network_worker(void *arg) {
    struct dripbox_server *dripbox_server = arg;
    struct tcp_listener *listener = &dripbox_server->listener;
    time_t pingtime = 0;
    const time_t dripbox_ping_interval = 1;

    socket_blocking(&listener->as_socket, false);
    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (!dripbox_server_quit) {
        using_monitor(&dripbox_server->m_users) {
            for (int i = 0; i < hash_table_capacity(dripbox_server->ht_users); i++) {
                struct hash_entry_t *entry;
                if (!hash_set_try_entry(dripbox_server->ht_users, i, &entry)) continue;

                const var kvp = (UserKVP*) entry->value;
                struct user *user = &kvp->value;
                if (user->username.length <= 0) continue;

                struct socket client = user->socket;
                if (client.sock_fd == -1) continue;

                if (!socket_pending_read(&client, 0)) continue;
                dripbox_server_handle_client_massage(dripbox_server, user);
            }
        }
        using_monitor(&dripbox_server->m_replicas) {
            for (int i = 0; i < hash_set_capacity(dripbox_server->ht_replicas); i++) {
                struct hash_entry_t *entry;
                if (!hash_set_try_entry(dripbox_server->ht_replicas, i, &entry)) continue;

                const var kvp = (ReplicaKVP*) entry->value;
                struct replica *replica = &kvp->value;
                if ((dripbox_server->election_state == ELECTION_STATE_NONE) &&
                    (dripbox_server->server_role == REPLICA_ROLE) &&
                    (replica->uuid.integer == dripbox_server->coordinator_uuid.integer)) {
                    const time_t elapsed = time(NULL) - pingtime;
                    if (elapsed > dripbox_ping_interval) {
                        diag(LOG_INFO, "Ping");
                        time(&pingtime);
                        socket_write_struct(&replica->socket, ((struct dripbox_msg_header) {
                            .version = 1,
                            .type = DRIP_MSG_NOOP,
                        }), 0);
                    }
                    if (replica->socket.error.code != 0 && replica->socket.error.code != EAGAIN) {
                        dripbox_server_start_election(dripbox_server);
                        continue;
                    }
                }
                if (replica->socket.sock_fd < 0) continue;

                if (!socket_pending_read(&replica->socket, 0)) continue;
                dripbox_server_handle_replica_massage(dripbox_server, replica);
            }
        }
    }
    return NULL;
}

void dripbox_server_handle_client_list(struct user *user) {
    struct dirent **server_files;
    const struct z_string client_dir = path_combine(dripbox_server.userdata_dir, user->username);
    const int files_count = scandir(client_dir.data, &server_files, dripbox_dirent_is_file, alphasort);

    struct dripbox_file_stat dripbox_sts[files_count];
    memset(dripbox_sts, 0, sizeof dripbox_sts);

    for (int i = 0; i < files_count; i++) {
        const struct dirent *entry = server_files[i];
        struct dripbox_file_stat *dripbox_st = &dripbox_sts[i];
        const struct z_string file_path = path_combine(client_dir, entry->d_name);

        struct stat st = {};
        if (stat(file_path.data, &st) >= 0) {
            dripbox_st->ctime = st.st_ctime;
            dripbox_st->atime = st.st_atime;
            dripbox_st->mtime = st.st_mtime;
            dripbox_st->checksum = dripbox_file_checksum(file_path.data);
        }
        else {
            diagf(LOG_INFO, "%s %s\n", strerror(errno), file_path.data);
        }
        strncpy(dripbox_st->name, entry->d_name, sizeof entry->d_name - 1);
    }

    struct socket *client = &user->socket;

    socket_write_struct(client, ((struct dripbox_msg_header) {
        .version = 1,
        .type = DRIP_MSG_LIST,
    }), 0);

    socket_write_struct(client, ((struct dripbox_list_header) {
        .file_list_length = files_count,
    }), 0);

    socket_write(client, size_and_address(dripbox_sts), 0);
}

void dripbox_server_handle_add_replica(struct dripbox_server *dripbox_server, struct socket *sock) {
    const var add_replica_header = socket_read_struct(sock, struct dripbox_add_replica_header, 0);
    if (zero_initialized(add_replica_header)) return;

    const struct uuidv7 server_uuid = add_replica_header.server_uuid;

    struct replica replica = {
        .uuid = server_uuid,
        .socket = *sock,
    };

    const var a = hash_table_allocator(dripbox_server->ht_replicas);
    char *remote_ip = ipv4_cstr(sock->addr, a);
    if (hash_table_insert(&dripbox_server->ht_replicas, remote_ip, replica)) {
        diagf(LOG_INFO, "Recived replica: Id=%s remote=%s\n",
            uuidv7_to_string(replica.uuid.as_uuid).data,
            remote_ip
        );
    }
    else {
        diagf(LOG_INFO, "Recived replica: Id=%s remote=%s (already exists)\n",
            uuidv7_to_string(replica.uuid.as_uuid).data,
            remote_ip
        );
    }

    if(election_higher_id(replica.uuid, dripbox_server->coordinator_uuid) && election_higher_id(replica.uuid, dripbox_server->uuid)) {
        diagf(LOG_INFO, "New coordinator: %s\n", uuidv7_to_string(replica.uuid.as_uuid).data);
        dripbox_server->coordinator_uuid = replica.uuid;
    }

    if (dripbox_server->coordinator_uuid.integer == dripbox_server->uuid.integer) {
        dripbox_server->server_role = MASTER_ROLE;
    }
    else {
        dripbox_server->server_role = REPLICA_ROLE;
    }

    // This useless piece of code holds the fabric of the universe together
    // YOU KNOW WHAT? FUCK UNDEFINED BEHAVIOUR FUCK SUPER SAYANS AND FUCK YOU!!!
    int devnull = open("/dev/null", O_WRONLY);
    dprintf(devnull, "%d\n", dripbox_server->server_role);
    close(devnull);
}

static void dripbox_handle_replica_upload(struct dripbox_server *dripbox_server, struct socket *sock) {
    const var upload_header = socket_read_struct(sock, struct dripbox_upload_header, 0);
    const struct string_view relative_path = sv_new(
        upload_header.file_name_length,
        socket_read_array(sock, char, upload_header.file_name_length + 1, 0)
    );
    const uint8_t client_checksum = socket_read_struct(sock, uint8_t, 0);
    struct sv_split_iterator parts = sv_split(relative_path, "/");
    iterator_skip(&parts.iterator, 2);

    const struct z_string dir_path = path_combine(dripbox_server->userdata_dir, parts._current);
    int result = mkdir(dripbox_server->userdata_dir.data, S_IRWXU | S_IRWXG | S_IRWXO);
    result = mkdir(dir_path.data, S_IRWXU | S_IRWXG | S_IRWXO);
    if (result < 0 && errno != EEXIST) {
        ediagf("mkdir: "sv_fmt"\n", (int)sv_deconstruct(dir_path));
        return;
    }
    if (stat(relative_path.data, &(struct stat){}) < 0) goto upload_file;
    if (dripbox_file_checksum(relative_path.data) == client_checksum) {
        socket_redirect_to_file(sock, "/dev/null", upload_header.payload_length);
        return;
    }
upload_file:
    socket_redirect_to_file(sock, relative_path.data, upload_header.payload_length);
    if (sock->error.code != 0) {
        diagf(LOG_INFO, "%s\n", strerror(sock->error.code));
    }
}

void dripbox_server_start_election(struct dripbox_server *dripbox_server) {
    // Only start election if no election is running
    if(dripbox_server->election_state == ELECTION_STATE_NONE) {
        for (int i = 0; i < hash_set_capacity(dripbox_server->ht_replicas); i++) {
            struct hash_entry_t *entry;
            if (!hash_set_try_entry(dripbox_server->ht_replicas, i, &entry)) continue;

            const var kvp = (ReplicaKVP*) entry->value;
            struct replica *replica = &kvp->value;
            if (replica->socket.sock_fd == -1) continue;
            // Skiping newer replicas since they will be notified by the coordinator message anyway
            if (dripbox_server->uuid.integer < replica->uuid.integer) continue;

            struct string36 uuid_str = uuidv7_to_string(replica->uuid.as_uuid);
            diagf(LOG_INFO, "Start election on replica %s\n", uuid_str.data);
            socket_write_struct(&replica->socket, ((struct dripbox_msg_header) {
                .version = 1,
                .type = DRIP_MSG_ELECTION
            }), 0);
        }
        dripbox_server->election_state = ELECTION_STATE_RUNNING;
        // If previous election coroutine is completed, start a new one otherwise do nothing since it's already running
        // this can happen in another thread otherwise this check would be redundant
        // since we already check for the election state before
        if(dripbox_server->election_coroutine_handle.co_state == CO_COMPLETED_SUCESSFULY) {
            dripbox_server->election_coroutine_handle.co_state = CO_CREATED;
        }
        if (dripbox_server->election_coroutine_handle.co_state == CO_CREATED) {
            // Schedule election coroutine
            diag(LOG_INFO, "Schedule election coroutine");
            struct coroutine *co_election = &dripbox_server->election_coroutine_handle;
            dripbox_server_election_coroutine(co_election, dripbox_server);
            fifo_push(dripbox_server->co_scheduler, co_election);
        }
    }
}

int dripbox_election_timeout = 1;
static void* dripbox_server_election_coroutine(struct coroutine *co,
                                               struct dripbox_server *dripbox_server) {
    struct coroutine timer;
    struct socket *s;
    co_on_init(co) {
        s = &dripbox_server->listener.as_socket;
        timer = co_new(&mallocator, 32);
        co_delay_seconds(&timer, dripbox_election_timeout);
    }

    COROUTINE(co, dripbox_server_election_coroutine, timer, dripbox_server, s) {
        while (dripbox_server->election_state != ELECTION_STATE_NONE) {
            if (timer.co_state == CO_COMPLETED_SUCESSFULY) {
                // If timeout is reached, you won the election
                diag(LOG_INFO, "Timeout is reached, you won the election");
                dripbox_server->server_role = MASTER_ROLE;
                dripbox_server->coordinator_uuid = dripbox_server->uuid;
                packed_tuple(struct dripbox_msg_header, struct dripbox_coordinator_header) coordinator_header = {
                    { .version = 1, .type = DRIP_MSG_COORDINATOR },
                    {
                        .coordinator_uuid = dripbox_server->uuid,
                        .coordinator_inaddr = ((struct sockaddr_in*)dripbox_server->interface_address.ifa_addr)->sin_addr.s_addr
                    },
                };
                // Notify dns
                socket_write_struct(&dripbox_server->dns_socket, coordinator_header, 0);
                // Notify all replicas
                while (!monitor_try_enter(&dripbox_server->m_replicas)) {
                    co_yield(co);
                    continue;
                }
                for (int i = 0; i < hash_set_length(dripbox_server->ht_replicas); i++) {
                    struct hash_entry_t *entry;
                    if (!hash_set_try_entry(dripbox_server->ht_replicas, i, &entry)) continue;

                    diag(LOG_INFO, "Notify replica");
                    const var kvp = (ReplicaKVP*) entry->value;
                    struct replica *replica = &kvp->value;
                    if (replica->socket.sock_fd == - 1) continue;
                    socket_write_struct(&replica->socket, coordinator_header, 0);
                }
                monitor_exit(&dripbox_server->m_replicas);
                // Notify all clients
                while (!monitor_try_enter(&dripbox_server->m_users)) {
                    co_yield(co);
                    continue;
                }
                for (int i = 0; i < hash_set_length(dripbox_server->ht_users); i++) {
                    struct hash_entry_t *entry;
                    if (!hash_set_try_entry(dripbox_server->ht_users, i, &entry)) continue;

                    diag(LOG_INFO, "Notify client");
                    const var kvp = (UserKVP*) entry->value;
                    struct user *user = &kvp->value;
                    if (user->socket.sock_fd == - 1) continue;
                    socket_write_struct(&user->socket, coordinator_header, 0);
                }
                monitor_exit(&dripbox_server->m_users);
                dripbox_server->election_state = ELECTION_STATE_NONE;
            }
            co_yield(co);
            co_resume(&timer);
        }
        diag(LOG_INFO, "Exit election coroutine");
    }
    co_delete(&mallocator, &timer);
    return NULL;
}

void dripbox_server_upload_user_to_replica(const struct dripbox_server *dripbox_server, struct replica *replica) {
    typedef packed_tuple(struct dripbox_msg_header, struct dripbox_list_user_header) list_user_msg_t;
    struct socket *sock = &replica->socket;
    for (int i = 0; i < hash_set_capacity(dripbox_server->ht_users); i++) {
        struct hash_entry_t *entry;
        if (!hash_set_try_entry(dripbox_server->ht_users, i, &entry)) continue;

        const var kvp = (UserKVP*) entry->value;
        const struct user *user = &kvp->value;
        if (user->socket.sock_fd < 0) continue;

        struct z_string user_dir = path_combine(dripbox_server->userdata_dir, user->username);
        diagf(LOG_INFO, "Replicating user "sv_fmt"\n", (int)sv_deconstruct(user->username));
        struct dirent **client_entries;
        const int client_entries_count = scandir(user_dir.data, &client_entries, dripbox_dirent_is_file, alphasort);
        printf("Client Entries: %d\n", client_entries_count);
        const var client_set = array_stack(struct string256_checksum, client_entries_count);
        for (int j = 0; j < client_entries_count; j++) {
            struct dirent *dirent = client_entries[j];
            const struct z_string path = path_combine(user_dir, dirent->d_name);
            client_set[j] = (struct string256_checksum) {
                .checksum = dripbox_file_checksum(path.data),
            };
            strncpy(client_set[j].name.data, dirent->d_name, sizeof dirent->d_name);
        }
        socket_write_struct(sock, ((list_user_msg_t) {
            { .version = 1, .type = DRIP_MSG_LIST_USER },
            { .username_length = user->username.length, .file_list_length = array_length(client_set) }
        }), 0);
        socket_write(sock, sv_deconstruct(user->username), 0);
        socket_write(sock, array_size(client_set), (uint8_t*)client_set, 0);
    }
}

void dripbox_server_handle_list_user(const struct dripbox_server *dripbox_server, struct socket *sock) {
    const var msg_header = socket_read_struct(sock, struct dripbox_list_user_header, 0);
    const int32_t username_length = msg_header.username_length;
    const int32_t file_count = msg_header.file_list_length;
    struct string_view username = sv_new(username_length, socket_read_array(sock, char, username_length, 0));
    struct string256_checksum *server_set = socket_read_array(sock, struct string256_checksum, file_count, 0);
    if (file_count <= 0) return;

    struct dirent **client_entries;
    struct z_string user_dir = path_combine(dripbox_server->userdata_dir, username);
    int my_entries_count = scandir(user_dir.data, &client_entries, dripbox_dirent_is_file, alphasort);
    if (my_entries_count < 0) my_entries_count = 0;
    var client_set = array_stack(struct string256_checksum, my_entries_count);
    for (int j = 0; j < my_entries_count; j++) {
        struct dirent *dirent = client_entries[j];
        const struct z_string path = path_combine(user_dir, dirent->d_name);
        client_set[j] = (struct string256_checksum) {
            .checksum = dripbox_file_checksum(path.data),
        };
        strncpy(client_set[j].name.data, dirent->d_name, sizeof dirent->d_name);
    }
    using_allocator_temp_arena {
        struct allocator *tempa = &allocator_temp_arena()->allocator;
        // Download = { file | file ∈ Server \ Client }
        var to_download = array_set_difference(server_set, client_set, file_name_checksum_equals, tempa);
        diagf(LOG_INFO, "Downloading %ld files\n", array_length(to_download));
        // Delete = { file | file ∈ (Client ↦ file_name) \ (Server ↦ file_name) }
        var to_delete = array_set_difference(client_set, server_set, file_name_checksum_name_equals, tempa);
        diagf(LOG_INFO, "Deleting %ld files\n", array_length(to_delete));

        for (int j = 0; j < array_length(to_delete); j++) {
            struct string256_checksum item = to_delete[j];
            const struct z_string path = path_combine(user_dir, item.name.data);
            if (unlink(path.data) < 0) {
                diagf(LOG_ERROR, "%s %s\n", strerror(errno), item.name.data);
                continue;
            }
            diagf(LOG_INFO, "Deleted %s\n", item.name.data);
        }

        for (int j = 0; j < array_length(to_download); j++) {
            struct string256_checksum item = to_download[j];
            const struct z_string path = path_combine(username, item.name.data);
            if(dripbox_download_replica_file(dripbox_server, sock, path) < 0) {
                diagf(LOG_ERROR, "Failed to Download %s\n", path.data);
            }
        }
    }
}

int dripbox_download_replica_file(const struct dripbox_server *dripbox_server, struct socket *sock,
                                  struct z_string filename) {

    typedef packed_tuple(struct dripbox_msg_header, struct dripbox_download_header) download_msg_t;
    typedef packed_tuple(struct dripbox_msg_header, struct dripbox_bytes_header) bytes_msg_t;

    socket_write_struct(sock, ((download_msg_t) {
        { .version = 1, .type = DRIP_MSG_DOWNLOAD },
        { .file_name_length = filename.length },
    }), 0);
    socket_write(sock, sv_deconstruct(filename), 0);

    const var msg_header = socket_read_struct(sock, bytes_msg_t, 0);
    if (!dripbox_expect_version(sock, msg_header.item1.version, 1)) return -1;
    if (!dripbox_expect_msg(sock, msg_header.item1.type, DRIP_MSG_BYTES)) return -1;

    const struct z_string dir_path = path_combine(dripbox_server->userdata_dir, sv_token(filename, "/")[0]);
    if (mkdir(dripbox_server->userdata_dir.data, S_IRWXU | S_IRWXG | S_IRWXO) < 0 && errno != EEXIST) {
        ediagf("mkdir: %s\n", dir_path.data);
        return -1;
    }

    if (mkdir(dir_path.data, S_IRWXU | S_IRWXG | S_IRWXO) < 0 && errno != EEXIST) {
        ediagf("mkdir: %s\n", dir_path.data);
        return -1;
    }

    const struct z_string fullpath = path_combine(dripbox_server->userdata_dir, filename);
    const ssize_t ret = socket_redirect_to_file(sock, fullpath.data, msg_header.item2.length);
    if (ret < 0 || sock->error.code != 0) {
        ediagf("%s\n", fullpath.data);
        return -1;
    }
    return 0;
}

void dripbox_server_handle_replica_download(struct dripbox_server *dripbox_server, struct socket *sock) {
    const var download_header = socket_read_struct(sock, struct dripbox_download_header, 0);
    const struct string_view path = sv_new(
        download_header.file_name_length,
        socket_read_array(sock, char, download_header.file_name_length, 0)
    );
    const struct z_string fullpath = path_combine(dripbox_server->userdata_dir, path);

    struct stat st = {};
    if (stat(fullpath.data, &st) < 0) {
        dripbox_send_error(sock, errno, SV(path));
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        dripbox_send_error(sock, EISDIR, SV(path));
        return;
    }

    typedef packed_tuple(struct dripbox_msg_header, struct dripbox_bytes_header) bytes_msg_t;
    socket_write_struct(sock, ((bytes_msg_t) {
        { .version = 1, .type = DRIP_MSG_BYTES },
        { .length = st.st_size },
    }), 0);
    fd_redirect_from_file(sock->sock_fd, fullpath.data);
}

static void dripbox_server_shutdown(void) {
    dripbox_server_quit = true;
}
