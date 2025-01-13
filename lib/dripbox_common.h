#ifndef DRIPBOX_COMMON_H
#define DRIPBOX_COMMON_H

#include <dirent.h>
#include <stdio.h>
#include <stdint.h>
#include <string_view.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/version.h>

#define DRIPBOX_DNS_PORT 46921
#define DRIPBOX_REPLICA_PORT 6969
#define DRIPBOX_REPLICA_ENDPOINT ipv4_any(DRIPBOX_REPLICA_PORT)
#define DRIPBOX_REPLICA_MULTICAST_GROUP ipv4_address(239, 192, 1, 1)

enum drip_message_type {\
    DRIP_MSG_NOOP = 0,
    DRIP_MSG_LOGIN = 1,
    DRIP_MSG_UPLOAD = 2,
    DRIP_MSG_DOWNLOAD = 3,
    DRIP_MSG_DELETE = 4,
    DRIP_MSG_LIST = 5,
    DRIP_MSG_BYTES = 6,
    DRIP_MSG_ADD_REPLICA = 7,
    DRIP_MSG_COORDINATOR = 8,
    DRIP_MSG_ELECTION = 9,
    DRIP_MSG_LIST_USER = 11,
    DRIP_MSG_ERROR = 12,
    DRIP_MSG_DNS = 13,
    DRIP_MSG_IN_ADRESS = 14,
    // DRIP_MSG_SEND_CLIENT = 15,
    DRIP_MSG_COUNT,
};

enum election_state {
    ELECTION_STATE_NONE,
    ELECTION_STATE_RUNNING,
    ELECTION_STATE_WAITING_COORDINATOR,
};

struct uuid {
    union {
        uint8_t bytes[16];
        __uint128_t integer;
    };
};

struct uuidv7 {
    union {
        struct {
            uint64_t timestamp:48;
            uint64_t version:4;
            uint64_t rand_a:12;
            uint64_t var_a:2;
            uint64_t rand_b:12;
        };
        struct uuid as_uuid;
        uint8_t bytes[16];
        __uint128_t integer;
    };
};

struct string36 {
    char data[37];
};

struct string256 {
    char data[256];
};

struct dripbox_file_stat {
    char name[256]; //file name
    time_t ctime;
    time_t atime;
    time_t mtime;
    uint8_t checksum;
} __attribute__((packed));
struct dripbox_msg_header {
    uint8_t version;
    uint8_t type;
} __attribute__((packed));
struct dripbox_login_header {
    uint64_t length;
} __attribute__((packed));
struct dripbox_upload_header {
    uint64_t file_name_length;
    uint64_t payload_length;
} __attribute__((packed));
struct dripbox_bytes_header {
    uint64_t length;
} __attribute__((packed));
struct dripbox_download_header {
    uint64_t file_name_length;
} __attribute__((packed));
struct dripbox_delete_header {
    uint64_t file_name_length;
} __attribute__((packed));
struct dripbox_error_header {
    uint64_t error_length;
} __attribute__((packed));
struct dripbox_list_header {
    uint64_t file_list_length;
} __attribute__((packed));
struct dripbox_add_replica_header {
    struct uuidv7 server_uuid;
} __attribute__((packed));
struct dripbox_coordinator_header {
    struct uuidv7 coordinator_uuid;
    uint32_t coordinator_inaddr;
} __attribute__((packed));
struct dripbox_election_header {
    struct uuid server_uuid;
} __attribute__((packed));
struct dripbox_list_user_header {
    uint64_t username_length;
    uint32_t file_list_length;
} __attribute__((packed));
struct string256_checksum {
    struct string256 name;
    uint8_t checksum;
} __attribute__((packed));
struct dripbox_in_adress_header {
    uint32_t in_addr;
} __attribute__((packed));
struct dripbox_send_client_header {
    uint64_t client_username_len;
    uint32_t in_addr;
} __attribute__((packed));

struct file_name_checksum {
    struct string_view name;
    uint8_t checksum;
};

static bool file_name_checksum_name_equals(const void *a, const void *b) {
    struct file_name_checksum *a_ = (struct file_name_checksum *) a;
    struct file_name_checksum *b_ = (struct file_name_checksum *) b;
    return sv_equals(a_->name, b_->name);
}

static bool file_name_checksum_equals(const void *a, const void *b) {
    struct file_name_checksum *a_ = (struct file_name_checksum *) a;
    struct file_name_checksum *b_ = (struct file_name_checksum *) b;
    return sv_equals(a_->name, b_->name) && a_->checksum == b_->checksum;
}

uint32_t dns_in_adrr = INADDR_ANY;
uint16_t port = 25565;
char *mode = NULL;

enum { MODE_INVALID, MODE_CLIENT, MODE_SERVER, MODE_DNS } mode_type = MODE_INVALID;

static uint8_t dripbox_checksum(const uint8_t *ptr, size_t sz);

static uint8_t dripbox_file_checksum(const char *path);

static int dripbox_dirent_is_file(const struct dirent *name) {
    if (name->d_type == DT_DIR) {
        return 0;
    }
    return 1;
}

static const char *msg_type_cstr(const enum drip_message_type msg_type) {
    _Static_assert(DRIP_MSG_COUNT == 15, "Enumeration changed please update this function");
    switch (msg_type) {
    case DRIP_MSG_LIST: return "List";
    case DRIP_MSG_UPLOAD: return "Upload";
    case DRIP_MSG_DOWNLOAD: return "Download";
    case DRIP_MSG_LOGIN: return "Login";
    case DRIP_MSG_NOOP: return "Noop";
    case DRIP_MSG_BYTES: return "Bytes";
    case DRIP_MSG_DELETE: return "Delete";
    case DRIP_MSG_ADD_REPLICA: return "Add Replica";
    case DRIP_MSG_ERROR: return "Error";
    case DRIP_MSG_COORDINATOR: return "Coordinator";
    case DRIP_MSG_LIST_USER: return "List User";
    case DRIP_MSG_DNS: return "DNS";
    case DRIP_MSG_IN_ADRESS: return "Internet Adress";
    // case DRIP_MSG_SEND_CLIENT: return "Send Client";
    default: return "Invalid Message";
    }
}

static struct string_view dripbox_read_error(struct socket *s) {
    const struct dripbox_error_header error_header = socket_read_struct(s, struct dripbox_error_header, 0);
    diagf(LOG_INFO, "Error { Error Length: %ld }\n", error_header.error_length);
    const var errstr = socket_read_array(s, char, error_header.error_length, 0);
    if (s->error.code != 0) {
        return SV(strerror(s->error.code));
    }
    return sv_new(error_header.error_length, errstr);
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
    uint8_t buffer[1024] = {}, chk = 0;
    size_t file_size = st.st_size;
    while (file_size > 0) {
        const size_t len = min(sizeof buffer, file_size);
        const ssize_t got = fread(buffer, sizeof(uint8_t), len, file);
        if (got == 0) {
            break;
        }
        if (got < 0) {
            diagf(LOG_ERROR, "%s %s\n", strerror(errno), path);
            break;
        }
        chk -= dripbox_checksum(buffer, got);
        file_size -= got;
    }
    fclose(file);
    return chk;
}

#define dripbox_expect_msg(s__, actual__, expected__) \
    dripbox_expect_msg_((s__), (actual__), (expected__), __FILE__, __LINE__)

static bool dripbox_expect_msg_(struct socket *s, const enum drip_message_type actual, const enum drip_message_type expected,
    char *file, const int line) {
    if (s->error.code != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(s->error.code));
        diagf(LOG_ERROR, "Caller Location [%s:%d]\n", file, line);
        return false;
    }
    if (actual == DRIP_MSG_ERROR) {
        const struct string_view sv_error = dripbox_read_error(s);
        diagf(LOG_ERROR, "Dripbox error: "sv_fmt"\n", (int)sv_deconstruct(sv_error));
        diagf(LOG_ERROR, "Caller Location [%s:%d]\n", file, line);
        return false;
    }
    if (actual != expected) {
        diagf(LOG_ERROR, "Expected Message %s, got Message %s\n", msg_type_cstr(expected), msg_type_cstr(actual));
        diagf(LOG_ERROR, "Caller Location [%s:%d]\n", file, line);
        return false;
    }
    return true;
}

#define dripbox_expect_version(s__, actual__, expected__) \
    dripbox_expect_version_((s__), (actual__), (expected__), __FILE__, __LINE__)

static bool dripbox_expect_version_(const struct socket *s, const uint8_t actual, const uint8_t expected,
                                     char *file, const int line) {
    if (s->error.code != 0) {
        diagf(LOG_ERROR, "%s\n", strerror(s->error.code));
        diagf(LOG_ERROR, "Caller Location [%s:%d]\n", file, line);
        return false;
    }
    if (actual != expected) {
        diagf(LOG_ERROR, "Expected Version 0X%X, got Version 0X%X\n", expected, actual);
        diagf(LOG_ERROR, "Caller Location [%s:%d]\n", file, line);
        return false;
    }
    return true;
}

static void dripbox_send_error(struct socket *s, const int errnum, const struct string_view context) {
    const struct string_view buffer = sv_stack(1024);
    const struct string_view sv_error = sv_printf(buffer, "%s "sv_fmt"\n", strerror(errnum), context);
    if (sv_equals(sv_error, sv_empty)) {
        diagf(LOG_ERROR, "%s\n", strerror(errno));
        return;
    }

    if (errnum != 0) {
        diagf(LOG_ERROR, ""sv_fmt"\n", (int)sv_deconstruct(sv_error));
    }

    socket_write_struct(s, ((struct dripbox_msg_header) {
        .version = 1,
        .type = DRIP_MSG_ERROR,
    }), 0);

    socket_write_struct(s, ((struct dripbox_error_header) {
        .error_length = sv_error.length,
    }), 0);

    socket_write(s, sv_deconstruct(sv_error), 0);

    if (s->error.code != 0 && s->error.code != errnum) {
        diagf(LOG_ERROR, "%s", strerror(s->error.code));
    }
}

/// Copies a file from src to dst
/// @param src_path source file path
/// @param dst_path destination file path
/// @return true if the copy was successful, false otherwise
static ssize_t copy_file(const char *src_path, const char *dst_path) {
    scope(const int src_fd = open(src_path, O_RDONLY), src_fd >= 0 && close(src_fd)) {
        if (src_fd < 0) {
            return -1;
        }
        struct stat st = {};
        if (fstat(src_fd, &st) < 0) {
            return -1;
        }
        scope(const int dest_fd = open(dst_path, O_WRONLY | O_CREAT, st.st_mode), dest_fd >= 0 && close(dest_fd)) {
            if (dest_fd < 0) {
                return -1;
            }
            #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
            off_t offset = 0;
            while (offset < st.st_size) {
                const ssize_t got = sendfile(dest_fd, src_fd, &offset, st.st_size - offset);
                if (got == 0) { break; }
                if (got < 0) { return -1; }
            }
            #else
            diagf(LOG_INFO, "sendfile not supported for files (Ironic), falling back to read/write");
            char buffer[1024] = {};
            while (st.st_size > 0) {
                const size_t len = min(sizeof buffer, st.st_size);
                const ssize_t got = read(src_fd, buffer, len);
                if (got == 0) { break; }
                if (got < 0) { return -1; }
                const ssize_t sent = write(dest_fd, buffer, got);
                if (sent == 0) { break; }
                if (sent < 0) { return -1; }
                st.st_size -= got;
            }
            #endif
            return st.st_size;
        }
    }
    return -1;
}

static ssize_t fd_redirect_from_file(const int dst_fd, const char *src_path) {
    scope(const int src_fd = open(src_path, O_RDONLY), dst_fd >= 0 && close(dst_fd)) {
        if (dst_fd < 0) {
            return -1;
        }
        struct stat st = {};
        if (fstat(src_fd, &st) < 0) {
            return -1;
        }
        #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
        off_t offset = 0;
        while (offset < st.st_size) {
            const ssize_t got = sendfile(dst_fd, src_fd, &offset, st.st_size - offset);
            if (got == 0) { break; }
            if (got < 0) { return -1; }
        }
        #else
        diagf(LOG_INFO, "sendfile not supported for files (Ironic), falling back to read/write");
        char buffer[1024] = {};
        while (st.st_size > 0) {
            const size_t len = min(sizeof buffer, st.st_size);
            const ssize_t got = read(src_fd, buffer, len);
            if (got == 0) { break; }
            if (got < 0) { return -1; }
            const ssize_t sent = write(dst_fd, buffer, got);
            if (sent == 0) { break; }
            if (sent < 0) { return -1; }
            st.st_size -= got;
        }
        #endif
        return st.st_size;
    }
    return -1;
}

#define tm_long_datefmt "%d/%2d/%2d %2d:%2d.%2d"

#define tm_long_data_deconstruct(tm__) \
    ((tm__)->tm_year + 1900),\
    ((tm__)->tm_mon + 1),\
    ((tm__)->tm_mday),\
    ((tm__)->tm_hour),\
    ((tm__)->tm_min),\
    ((tm__)->tm_sec)

#define dripbox_file_stat_fmt \
    "{\n"\
    "  Name: %s,\n"\
    "  ctime: "tm_long_datefmt",\n"\
    "  atime: "tm_long_datefmt",\n"\
    "  mtime: "tm_long_datefmt",\n"\
    "  checksum: 0X%X\n"\
    "}"

enum transaction_result {
    TRNASACTION_FATAL_ERROR,
    TRNASACTION_STARTED,
    TRNASACTION_COMMITTED,
    TRNASACTION_ROLLEDBACK,
};

static const char* transaction_state_to_cstr(const enum transaction_result state) {
    switch (state) {
    case TRNASACTION_FATAL_ERROR: return "Fatal Error";
    case TRNASACTION_STARTED: return "Started";
    case TRNASACTION_COMMITTED: return "Committed";
    case TRNASACTION_ROLLEDBACK: return "Rolledback";
    default: return "Invalid Enum Value";
    }
}

static enum transaction_result dripbox_file_transaction_commit(const struct z_string dst_file) {
    const struct z_string dst_backup = zconcat(dst_file, ".swp");

    if (rename(dst_backup.data, dst_file.data) < 0) {
        ediagf("rename %s, %s\n", dst_backup.data, dst_file.data);
        return TRNASACTION_FATAL_ERROR;
    }

    diagf(LOG_INFO, "Swapped %s with %s\n", dst_backup.data, dst_file.data);
    return TRNASACTION_COMMITTED;
}

static enum transaction_result dripbox_file_transaction_rollback(const struct z_string dst_file) {
    const struct z_string dst_backup = zconcat(dst_file, ".swp");

    if (unlink(dst_backup.data) < 0) {
        ediagf("rename %s, %s\n", dst_backup.data, dst_file.data);
        return TRNASACTION_FATAL_ERROR;
    }

    diagf(LOG_INFO, "Rolled back %s\n", dst_file.data);
    return TRNASACTION_ROLLEDBACK;
}


static enum transaction_result dripbox_transaction_copy_file(const struct z_string src_path,
                                                             const struct z_string dst_path)
{
    const struct z_string dst_backup_file = zconcat(dst_path, ".swp");

    scope(const int dst_fd = open(dst_backup_file.data, O_CREAT | O_WRONLY), dst_fd >= 0 && close(dst_fd)) {
        if (dst_fd < 0) {
            ediagf("open %s, O_CREAT | O_WRONLY\n", dst_backup_file.data);
            return TRNASACTION_FATAL_ERROR;
        }

        diagf(LOG_INFO, "Created %s\n", dst_backup_file.data);
        if (fd_redirect_from_file(dst_fd, src_path.data) < 0) {
            ediagf("copy_file %s, %s\n", src_path.data, dst_path.data);
            return dripbox_file_transaction_rollback(dst_path);
        }
    }
    return dripbox_file_transaction_commit(dst_path);
}

// https://antonz.org/uuidv7/
static bool uuidv7_try_new(struct uuidv7 *uuid) {
    const int err = getentropy(uuid->bytes, 16);
    if (err != EXIT_SUCCESS) {
        return false;
    }

    struct timespec ts;
    const int ok = timespec_get(&ts, TIME_UTC);
    if (ok == 0) {
        return false;
    }
    const uint64_t timestamp = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

    uuid->bytes[0] = timestamp >> 40 & 0xFF;
    uuid->bytes[1] = timestamp >> 32 & 0xFF;
    uuid->bytes[2] = timestamp >> 24 & 0xFF;
    uuid->bytes[3] = timestamp >> 16 & 0xFF;
    uuid->bytes[4] = timestamp >> 8 & 0xFF;
    uuid->bytes[5] = timestamp & 0xFF;

    uuid->bytes[6] = uuid->bytes[6] & 0x0F | 0x70;
    uuid->bytes[8] = uuid->bytes[8] & 0x3F | 0x80;

    return true;
}

static struct uuidv7 uuidv7_new() {
    struct uuidv7 uuid;
    const bool success = uuidv7_try_new(&uuid);
    assert(success && "uuidv7_try_new");
    return uuid;
}

static bool uuidv7_comparer_equals(const void *a, const void *b) {
    const struct uuid *uuid_a = (struct uuid *) a;
    const struct uuid *uuid_b = (struct uuid *) b;
    return uuid_a->integer == uuid_b->integer;
}

static int32_t uuidv7_hash(const void *element) {
    const struct uuid *uuid = (struct uuid *) element;
    int32_t hash = 0x12345678;
    for (int i = 0; i < 16; i++) {
        hash ^= uuid->bytes[i];
        hash *= 0x5bd1e995;
        hash ^= hash >> 15;
    }
    return hash;
}

static struct string36 uuidv7_to_string(const struct uuid uuid) {
    struct string36 uuid_str;
    const uint32_t data1 = *(uint32_t*)&uuid.bytes[0];
    const uint16_t data2 = *(uint16_t*)&uuid.bytes[4];
    const uint16_t data3 = *(uint16_t*)&uuid.bytes[6];
    sprintf(uuid_str.data,
    "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        data1, data2, data3,
        uuid.bytes[8],
        uuid.bytes[9],
        uuid.bytes[10],
        uuid.bytes[11],
        uuid.bytes[12],
        uuid.bytes[13],
        uuid.bytes[14],
        uuid.bytes[15]
    );
    return uuid_str;
}
#endif //DRIPBOX_COMMON_H
