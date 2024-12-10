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

static enum {
    MASTER_ROLE,
    REPLICA_ROLE
} server_role = MASTER_ROLE;

#define DRIPBOX_REPLICAS 2
#define DRIPBOX_REPLICA_PORT 6969
#define DRIPBOX_REPLICA_ENDPOINT ipv4_any(DRIPBOX_REPLICA_PORT)
#define DRIPBOX_REPLICA_MULTICAST_GROUP ipv4_address(239, 192, 1, 1)


struct user_t {
    struct string_view username;
    struct socket socket;
};

struct replica {
    struct uuid uuid;
    struct socket socket;
};

typedef hash_table(char *, struct user_t) UsersHashTable;
typedef key_value_pair(char *, struct user_t) UserKVP;
typedef hash_table(char*, struct replica) ReplicaHashTable;
typedef key_value_pair(char*, struct replica) ReplicaKVP;

const struct z_string g_userdata_dir = (struct z_string){
    .data = "userdata/",
    .length = sizeof "userdata/" - 1,
};

struct monitor g_users_monitor  = MONITOR_INITIALIZER;

struct queue server_coroutine_scheduler_queue;
ReplicaHashTable replicas_hash_table = NULL;
struct monitor g_replica_monitor = MONITOR_INITIALIZER;
struct uuid server_uuid;
packed_tuple(struct dripbox_msg_header, struct dripbox_add_replica_header) discovery_probe;

struct dripbox_server_context {
    struct tcp_listener *listener;
    UsersHashTable *hash_table;
};

// struct dripbox_server_context {
//     struct tcp_listener *listener;
//     UsersHashTable *ht_users;
//     ReplicaHashTable ht_replicas;
//     struct monitor *m_replicas;
//     struct monitor *m_users;
//     struct uuid uuid;
//     struct queue co_scheduler;
//     pthread_t incoming_connections_worker;
//     pthread_t network_worker;
//     packed_tuple(struct dripbox_msg_header, struct dripbox_add_replica_header) discovery_probe;
//     struct z_string userdata_dir;
//     enum { MASTER_ROLE, REPLICA_ROLE } server_role;
// };

static void dripbox_server_handle_client_login(UsersHashTable *hash_table, struct socket *client);

static void dripbox_server_handle_client_upload(const UsersHashTable *users_hash_table, struct user_t *user);

static void dripbox_server_handle_client_download(struct user_t *user);

static void dripbox_server_handle_client_delete(const UsersHashTable *hash_table, struct user_t *user);

static void dripbox_server_handle_client_list(struct user_t *user);

static void dripbox_server_handle_client_massage(UsersHashTable *hash_table, struct user_t *user);

static void *dripbox_server_incoming_connections_worker(const void *arg);

static void *dripbox_server_network_worker(const void *arg);

static void dripbox_server_handle_add_replica(struct socket *replica_sock);

static ssize_t dripbox_upload_client_file(
    struct socket *sock,
    struct string_view file_name,
    uint8_t checksum,
    struct z_string path
);

static void dripbox_delete_client_file(struct user_t *user, struct string_view file_name);

static void* server_discover_replicas(struct coroutine *co, const struct ifaddrs *addrs) {
    enum { nested_coroutine_size = 64 };

    struct ifaddrs server_addr;
    struct socket multicast_sock = co_assign_on_init(co, socket_new());


    struct socket_address *multicast_group_address = co_detach_on_init(co, struct socket_address,
        ((struct socket_address) {
            .sa = (void*)co_detach_on_init(co, struct sockaddr_in, sockaddr_in(DRIPBOX_REPLICA_MULTICAST_GROUP, DRIPBOX_REPLICA_PORT)),
            .addr_len = sizeof(struct sockaddr_in),
        }
    ));

    struct socket_address *remote =  co_detach_on_init(co, struct socket_address,
        ((struct socket_address) {
            .sa = (void*)co_detach_on_init(co, struct sockaddr_in, zero(struct sockaddr_in)),
            .addr_len = sizeof(struct sockaddr_in),
        }
    ));

    COROUTINE(co, server_discover_replicas,
              multicast_group_address, multicast_sock,
              remote,
              addrs, server_addr) {
        while (addrs) {
            if (addrs->ifa_addr && addrs->ifa_addr->sa_family == AF_INET && strcmp(addrs->ifa_name, "lo") != 0) {
                server_addr = *addrs;
                break;
            }
            addrs = addrs->ifa_next;
        }

        socket_open(&multicast_sock, AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        socket_bind(&multicast_sock, &ipv4_endpoint(INADDR_ANY, DRIPBOX_REPLICA_PORT));
        socket_blocking(&multicast_sock, false);
        socket_reuse_address(&multicast_sock, true);
        socket_join_multicast_group(&multicast_sock, (struct ip_mreq) {
            .imr_multiaddr = htonl(DRIPBOX_REPLICA_MULTICAST_GROUP),
            .imr_interface = INADDR_ANY,
        });
        socket_option(&multicast_sock, IPPROTO_IP, IP_MULTICAST_LOOP, false);

        socket_write_struct_to(&multicast_sock, discovery_probe, multicast_group_address, 0);
        while (true) {
            if(!socket_pending(&multicast_sock, 0)) {
                co_yield(co);
                continue;
            }

            var discovery_response = socket_read_struct_from(&multicast_sock, typeof(discovery_probe), remote, 0);
            if(!dripbox_expect_version(&multicast_sock, discovery_response.item1.version, 1)) continue;
            if(!dripbox_expect_msg(&multicast_sock, discovery_response.item1.type, MSG_ADD_REPLICA)) continue;

            char *remote_ip = ipv4_cstr(remote, &mallocator);
            using_monitor(&g_replica_monitor) {
                struct replica replica = {
                    .uuid = discovery_response.item2.server_uuid,
                    .socket = (struct socket) {
                        .sock_fd = -1,
                        .addr = allocator_copy(hash_table_allocator(replicas_hash_table), remote, sizeof *remote),
                    },
                };
                if (hash_table_insert(&replicas_hash_table, remote_ip, replica)) {
                    diagf(LOG_INFO, "Recived replica: Id=%s remote=%s\n",
                        uuidv7_to_string(discovery_response.item2.server_uuid).data,
                        remote_ip
                    );
                }
                else {
                    diagf(LOG_INFO, "Recived replica: Id=%s remote=%s (already exists)\n",
                        uuidv7_to_string(discovery_response.item2.server_uuid).data,
                        remote_ip
                    );
                }
            }
        }
    }
    return NULL;
}

static void *dripbox_server_connect_replica_async(struct coroutine *co,
                                                  struct socket *replica_sock,
                                                  struct socket_address *remote) {
    COROUTINE(co, dripbox_server_connect_replica_async, replica_sock, remote) {
        diagf(LOG_INFO, "Connecting to replica\n");
        bool connected = socket_connect(replica_sock, remote);
        while (replica_sock->error.code == EAGAIN) {
            co_yield(co);
            replica_sock->error.code = 0;
            connected = socket_connect(replica_sock, remote);
        }
        socket_write_struct(replica_sock, discovery_probe, 0);

        if (replica_sock->error.code == 0) {
            diagf(LOG_INFO, "Connected to replica\n");
        }
        else {
            ediagf("dripbox_server_connect_replica_async\n");
        }
        co_return(co, connected);
    }
    return NULL;
}

static void *dripbox_server_connect_replicas(struct coroutine *co) {
    COROUTINE(co, dripbox_server_connect_replicas) {
        while (!dripbox_server_quit) {
            using_monitor(&g_replica_monitor) {
                for (int i = 0; i < hash_set_capacity(replicas_hash_table); i++) {
                    struct hash_entry_t *entry;
                    if (!hash_set_try_entry(replicas_hash_table, i, &entry)) continue;

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
                        fifo_push(server_coroutine_scheduler_queue, co_connect_async);
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

    server_uuid = uuidv7_new();
    discovery_probe = (typeof(discovery_probe)) {
        { .version = 1, .type = MSG_ADD_REPLICA },
        { .server_uuid = server_uuid },
    };

    void* list = dynamic_array_new(struct coroutine*, &mallocator);
    server_coroutine_scheduler_queue = dynamic_array_fifo(&list);
    replicas_hash_table = (void*)hash_table_new(char*, struct replica,\
                                                string_hash, string_comparer_equals,\
                                                &mallocator);

    struct tcp_listener listener = tcp_listener_new();
    tcp_listener_bind(&listener, AF_INET, &ipv4_endpoint(ip, port));
    tcp_listener_listen(&listener, SOMAXCONN);
    if (listener.error.code != 0) {
        diagf(LOG_INFO, "%s\n", strerror(listener.error.code));
        return 1;
    }

    pthread_t incoming_connections_worker_id, nextwork_worker_id;
    var users_hash_table = hash_table_new(char*, struct user_t,
                                          string_hash, string_comparer_equals,
                                          &mallocator);
    struct dripbox_server_context ctx = {
        .listener = &listener,
        .hash_table = (UsersHashTable *) &users_hash_table,
    };

    struct coroutine *co_add_replicas = &co_stack(4096);
    server_discover_replicas(co_add_replicas, addrs);
    fifo_push(server_coroutine_scheduler_queue, co_add_replicas);

    struct coroutine *co_connect_replicas = &co_stack(4096);
    dripbox_server_connect_replicas(co_connect_replicas);
    fifo_push(server_coroutine_scheduler_queue, co_connect_replicas);

    pthread_create(&incoming_connections_worker_id, NULL, (void *) dripbox_server_incoming_connections_worker, &ctx);
    pthread_create(&nextwork_worker_id, NULL, (void *) dripbox_server_network_worker, &ctx);

    struct coroutine shcedureler = co_stack(128);
    while (!dripbox_server_quit) {
        co_queue_dispatch(&shcedureler, server_coroutine_scheduler_queue);
    }

    pthread_join(incoming_connections_worker_id, NULL);
    pthread_join(nextwork_worker_id, NULL);

    return 0;
}

static void dripbox_server_handle_client_login(UsersHashTable *hash_table, struct socket *client) {
    const var login_header = socket_read_struct(client, struct dripbox_login_header, 0);
    const struct string_view username = sv_new(
        login_header.length,
        socket_read_array(client, char, login_header.length, 0)
    );

    struct allocator *a = hash_set_allocator(*hash_table);
    const struct user_t _user = {
        .username = (struct string_view){
            .data = cstr_sv(username, a),
            .length = username.length,
        },
        .socket = *client,
    };

    const char *adress_cstr = ipv4_cstr(client->addr, a);
    hash_table_update(hash_table, adress_cstr, _user);

    const struct z_string dirpath = path_combine(g_userdata_dir, username);
    if (stat(g_userdata_dir.data, &(struct stat){}) < 0) {
        mkdir(g_userdata_dir.data, S_IRWXU | S_IRWXG | S_IRWXO);
    }
    if (stat(dirpath.data, &(struct stat){}) < 0) {
        mkdir(dirpath.data, S_IRWXU | S_IRWXG | S_IRWXO);
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
            .type = MSG_UPLOAD,
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

void dripbox_delete_client_file(struct user_t *user, const struct string_view file_name) {
    struct socket *client = &user->socket;

    socket_write_struct(client, ((struct dripbox_msg_header) {
        .version = 1,
        .type = MSG_DELETE,
    }), 0);

    socket_write_struct(client, ((struct dripbox_delete_header) {
        .file_name_length = file_name.length,
    }), 0);

    socket_write(client, sv_deconstruct(file_name), 0);

    if (client->error.code != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(client->error.code));
    }
}

static void dripbox_server_handle_client_upload(const UsersHashTable *users_hash_table, struct user_t *user) {
    struct socket *client = &user->socket;
    const var upload_header = socket_read_struct(client, struct dripbox_upload_header, 0);
    const struct string_view file_name = sv_new(
        upload_header.file_name_length,
        socket_read_array(client, char, upload_header.file_name_length, 0)
    );
    const uint8_t client_checksum = socket_read_struct(client, uint8_t, 0);
    const struct string_view username = user->username;
    const struct z_string dir_path = path_combine(g_userdata_dir, username);
    if (mkdir(dir_path.data, S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
        ediagf("mkdir: "sv_fmt"\n", dir_path);
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

    const var users_header = hash_table_header(*users_hash_table);
    for (int i = 0; i < users_header->capacity; i++) {
        struct hash_entry_t *entry;
        if (!hash_set_try_entry(*users_hash_table, i, &entry)) continue;

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

    for (int i = 0; i < hash_set_capacity(replicas_hash_table); i++) {
        struct hash_entry_t *entry;
        if (!hash_set_try_entry(replicas_hash_table, i, &entry)) continue;

        const var kvp = (ReplicaKVP*) entry->value;
        struct replica *replica = &kvp->value;
        assert(replica->socket.sock_fd >= 0 && "How ???");
        dripbox_upload_client_file(&replica->socket, file_name, client_checksum, full_path);
        diagf(LOG_INFO, "Replicated "sv_fmt" Checksum: 0X%X\n",
            (int)sv_deconstruct(file_name),
            client_checksum
        );
    }
}

void dripbox_server_handle_client_download(struct user_t *user) {
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
    const struct z_string path = path_combine(g_userdata_dir, username, file_name);

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
            .type = MSG_BYTES,
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

void dripbox_server_handle_client_delete(const UsersHashTable *hash_table, struct user_t *user) {
    struct socket *client = &user->socket;
    const var delete_header = socket_read_struct(client, struct dripbox_delete_header, 0);
    const struct string_view file_name = sv_new(
        delete_header.file_name_length,
        socket_read_array(client, char, delete_header.file_name_length, 0)
    );
    const struct string_view username = user->username;
    const struct z_string path = path_combine(g_userdata_dir, username, file_name);

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

    for (int i = 0; i < hash_table_capacity(*hash_table); i++) {
        struct hash_entry_t *entry;
        if (!hash_set_try_entry(*hash_table, i, &entry)) continue;

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

static void dripbox_server_handle_client_massage(UsersHashTable *hash_table, struct user_t *user) {
    struct socket *client = &user->socket;
    const var msg_header = socket_read_struct(client, struct dripbox_msg_header, 0);
    if (zero_initialized(msg_header)) return;
    if (!dripbox_expect_version(client, msg_header.version, 1)) return;

    const char *msg_type = msg_type_cstr(msg_header.type);
    diagf(LOG_INFO, "Received Message %s\n", msg_type);
    switch (msg_header.type) {
    case MSG_NOOP: break;
    case MSG_LOGIN: {
        dripbox_server_handle_client_login(hash_table, client);
        break;
    }
    case MSG_UPLOAD: {
        dripbox_server_handle_client_upload(hash_table, user);
        break;
    }
    case MSG_DOWNLOAD: {
        dripbox_server_handle_client_download(user);
        break;
    }
    case MSG_DELETE: {
        dripbox_server_handle_client_delete(hash_table, user);
        break;
    }
    case MSG_LIST: {
        dripbox_server_handle_client_list(user);
        break;
    }
    case MSG_ADD_REPLICA: {
        dripbox_server_handle_add_replica(&user->socket);
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

    diag(LOG_INFO, "Processed message\n");
}

void *dripbox_server_incoming_connections_worker(const void *arg) {
    const struct dripbox_server_context *context = arg;
    struct tcp_listener *listener = context->listener;
    var addr = ipv4_endpoint(0, 0);

    socket_blocking(&listener->as_socket, false);
    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (!dripbox_server_quit) {
        struct socket client = {};
        while (tcp_server_incoming_next(listener, &client, &addr)) {
            struct user_t u = { .socket = client };
            using_monitor(&g_users_monitor) dripbox_server_handle_client_massage(context->hash_table, &u);
        }
    }
    return NULL;
}

void *dripbox_server_network_worker(const void *arg) {
    const struct dripbox_server_context *context = arg;
    struct tcp_listener *listener = context->listener;

    socket_blocking(&listener->as_socket, false);
    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (!dripbox_server_quit) {
        using_monitor(&g_users_monitor) {
            UsersHashTable *users = context->hash_table;
            for (int i = 0; i < hash_table_capacity(*users); i++) {
                struct hash_entry_t *entry;
                if (!hash_set_try_entry(*users, i, &entry)) continue;

                const var kvp = (UserKVP*) entry->value;
                struct user_t *user = &kvp->value;
                if (user->username.length <= 0) { continue; }

                struct socket client = user->socket;
                if (client.sock_fd == -1) { continue; }

                if (!socket_pending(&client, 0)) { continue; }
                dripbox_server_handle_client_massage(users, user);
            }
        }
        using_monitor(&g_replica_monitor) {
            for (int i = 0; i < hash_set_capacity(replicas_hash_table); i++) {
                struct hash_entry_t *entry;
                if (!hash_set_try_entry(replicas_hash_table, i, &entry)) continue;

                const var kvp = (ReplicaKVP*) entry->value;
                struct replica *replica = &kvp->value;
                struct socket *replica_sock = &replica->socket;
                const struct socket_address *remote = replica_sock->addr;

                if (replica_sock->sock_fd == -1) {
                    //TODO: Reconsider this
                    diagf(LOG_WARNING, "Replica socket was not connected when server recieved a file\n");
                    diagf(LOG_WARNING, "Doing a blocking connect on the server thread, reconsider this\n");
                    socket_open(replica_sock, AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    socket_reuse_address(replica_sock, true);
                    socket_adress_set_port(remote, port);
                    diagf(LOG_INFO, "Connecting to replica\n");
                    socket_connect(replica_sock, remote);
                    if (replica_sock->error.code == 0) {
                        diagf(LOG_INFO, "Connected to replica\n");
                    }
                    else {
                        ediagf("dripbox_server_connect_replica_async\n");
                        monitor_return(&g_replica_monitor);
                    }
                }

                if (!socket_pending(replica_sock, 0)) { continue; }
                dripbox_server_handle_client_massage(context->hash_table, &(struct user_t ){ .socket = *replica_sock } );
            }
        }
    }
    return NULL;
}

void dripbox_server_handle_client_list(struct user_t *user) {
    struct dirent **server_files;
    const struct z_string client_dir = path_combine(g_userdata_dir, user->username);
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
        .type = MSG_LIST,
    }), 0);

    socket_write_struct(client, ((struct dripbox_list_header) {
        .file_list_length = files_count,
    }), 0);

    socket_write(client, size_and_address(dripbox_sts), 0);
}

void dripbox_server_handle_add_replica(struct socket *replica_sock) {
    const var add_replica_header = socket_read_struct(replica_sock, struct dripbox_add_replica_header, 0);
    if (zero_initialized(add_replica_header)) return;

    const struct uuid server_uuid = add_replica_header.server_uuid;

    struct replica replica = {
        .uuid = server_uuid,
        .socket = *replica_sock,
    };

    using_monitor(&g_replica_monitor) {
        const var a = hash_table_allocator(replicas_hash_table);
        char *remote_ip = ipv4_cstr(replica_sock->addr, a);
        if (hash_table_insert(&replicas_hash_table, remote_ip, replica)) {
            diagf(LOG_INFO, "Recived replica: Id=%s remote=%s\n",
                uuidv7_to_string(replica.uuid).data,
                remote_ip
            );
        }
        else {
            diagf(LOG_INFO, "Recived replica: Id=%s remote=%s (already exists)\n",
                uuidv7_to_string(replica.uuid).data,
                remote_ip
            );
        }
    }
}

static void dripbox_server_shutdown(void) {
    dripbox_server_quit = true;
}
