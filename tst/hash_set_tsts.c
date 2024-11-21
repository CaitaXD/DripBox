#include <hash_set.h>
#include <string.h>


int32_t address_hash(const void *element) {
    uint32_t hash = *(uint32_t *) element;
    hash = ~hash + (hash << 15);
    hash = hash ^ hash >> 12;
    hash = hash + (hash << 2);
    hash = hash ^ hash >> 4;
    hash = hash * 2057;
    hash = hash ^ hash >> 16;
    return hash;
}

bool address_equals(const void *a, const void *b) {
    return *(intptr_t *) a == *(intptr_t *) b;
}

int32_t hash_int_ptr(const void *element) {
    const int32_t *int_a = element;
    const int32_t hash = *int_a;
    return hash;
}

bool int_ptr_equals(const void *a, const void *b) {
    const int32_t *int_a = a;
    const int32_t *int_b = b;
    return *int_a == *int_b;
}

int main() {
    bool result = false;

    var people = hash_set_new(char*, string_hash, string_equals, &default_allocator);

    result = hash_set_insert(people, "John");
    assert(result);
    result = hash_set_contains(people, "John");
    assert(result);

    result = hash_set_insert(people, "Jane");
    assert(result);
    result = hash_set_insert(people, "Jayne");
    assert(result);
    result = hash_set_insert(people, "Johnny");
    assert(result);
    result = hash_set_insert(people, "Bob");
    assert(result);
    result = hash_set_insert(people, "Bismarck");
    assert(result);
    result = hash_set_insert(people, "Boss");
    assert(result);
    result = hash_set_insert(people, "Barbarosa");
    assert(result);
    result = hash_set_insert(people, "Molotov Cocktail");
    assert(result);
    result = hash_set_insert(people, "King Kong");
    assert(result);
    result = hash_set_insert(people, "Kinger Kong");
    assert(result);
    result = hash_set_insert(people, "Kinger Konger");
    assert(result);
    result = hash_set_insert(people, "Man");
    assert(result);
    result = hash_set_insert(people, "Woman");
    assert(result);
    result = hash_set_insert(people, "Foo");
    assert(result);

    var hs = hash_set_header(people);
    printf("Capacity: %zu\n", hs->capacity);
    printf("Length: %zu\n", hs->length);
    printf("Buckets: %zu\n", hs->buckets_length);
    for (size_t i = 0; i < hs->capacity; i++) {
        struct hash_entry_t *entry = _hash_set_entry_at_impl(hs, i, sizeof(char *));
        const ssize_t next = entry->next;
        if (HASH_ENTRY_IS_NULL(entry)) continue;
        printf("Index: %zu, ", i);
        printf("Hash: %d, ", entry->hash);
        printf("Value: %s, ", *(char **) entry->value);
        printf("Bucket: %zu, ", entry->hash % hs->buckets_length);
        printf(next == 0 ? "Next: -\n" : "Next: %zu\n", next - 1);
    }
    printf("\n");

    for (int i = 0; i < hash_set_length(people); i++) {
        printf("Item: %s\n", hash_set_at_index(people, i));
    }
    printf("\n");

    result = !hash_set_insert(people, "John");
    assert(result && "Duplicate insert should fail");
    result = !hash_set_insert(people, "Jane");
    assert(result && "Duplicate insert should fail");
    result = !hash_set_insert(people, "Jayne");
    assert(result && "Duplicate insert should fail");
    result = !hash_set_insert(people, "Johnny");
    assert(result && "Duplicate insert should fail");
    result = !hash_set_insert(people, "Bob");
    assert(result && "Duplicate insert should fail");
    result = !hash_set_insert(people, "Bismarck");
    assert(result && "Duplicate insert should fail");
    result = !hash_set_insert(people, "Boss");
    assert(result && "Duplicate insert should fail");
    result = !hash_set_insert(people, "Barbarosa");
    assert(result && "Duplicate insert should fail");
    result = !hash_set_insert(people, "Molotov Cocktail");
    assert(result && "Duplicate insert should fail");
    result = !hash_set_insert(people, "King Kong");
    assert(result && "Duplicate insert should fail");
    result = !hash_set_insert(people, "Kinger Konger");
    assert(result && "Duplicate insert should fail");
    result = !hash_set_insert(people, "Man");
    assert(result && "Duplicate insert should fail");
    result = !hash_set_insert(people, "Woman");
    assert(result && "Duplicate insert should fail");
    result = !hash_set_insert(people, "Foo");
    assert(result && "Duplicate insert should fail");
    result = !hash_set_insert(people, "Kinger Kong");
    assert(result && "Duplicate insert should fail");

    result = hash_set_remove(people, "John");
    assert(result && "Failed to remove");
    result = hash_set_remove(people, "Jane");
    assert(result && "Failed to remove");
    result = hash_set_remove(people, "Jayne");
    assert(result && "Failed to remove");
    result = hash_set_remove(people, "Johnny");
    assert(result && "Failed to remove");
    result = hash_set_remove(people, "Bob");
    assert(result && "Failed to remove");
    result = hash_set_remove(people, "Bismarck");
    assert(result && "Failed to remove");
    result = hash_set_remove(people, "Boss");
    assert(result && "Failed to remove");
    result = hash_set_remove(people, "Molotov Cocktail");
    assert(result && "Failed to remove");
    result = hash_set_remove(people, "King Kong");
    assert(result && "Failed to remove");
    result = hash_set_remove(people, "Kinger Konger");
    assert(result && "Failed to remove");
    result = hash_set_remove(people, "Man");
    assert(result && "Failed to remove");
    result = hash_set_remove(people, "Woman");
    assert(result && "Failed to remove");
    result = hash_set_remove(people, "Foo");
    assert(result && "Failed to remove");
    result = hash_set_remove(people, "Kinger Kong");
    assert(result && "Failed to remove");

    printf("Capacity: %zu\n", hs->capacity);
    printf("Length: %zu\n", hs->length);
    printf("Buckets: %zu\n", hs->buckets_length);
    for (size_t i = 0; i < hs->capacity; i++) {
        struct hash_entry_t *entry = _hash_set_entry_at_impl(hs, i, sizeof(char *));
        const ssize_t next = entry->next;
        if (next == -1) continue;
        printf("Index: %zu, ", i);
        printf("Hash: %d, ", entry->hash);
        printf("Value: %s, ", *(char **) entry->value);
        printf("Bucket: %zu, ", entry->hash % hs->buckets_length);
        printf(next == 0 ? "Next: -\n" : "Next: %zu\n", next - 1);
    }
    printf("\n");

    result = hash_set_remove(people, "Barbarosa");
    assert(result && "Failed to remove");

    printf("Capacity: %zu\n", hs->capacity);
    printf("Length: %zu\n", hs->length);
    printf("Buckets: %zu\n", hs->buckets_length);
    for (size_t i = 0; i < hs->capacity; i++) {
        struct hash_entry_t *entry = _hash_set_entry_at_impl(hs, i, sizeof(char *));
        const ssize_t next = entry->next;
        if (next == -1) continue;
        printf("Index: %zu, ", i);
        printf("Hash: %d, ", entry->hash);
        printf("Value: %s, ", *(char **) entry->value);
        printf("Bucket: %zu, ", entry->hash % hs->buckets_length);
        printf(next == 0 ? "Next: -\n" : "Next: %zu\n", next - 1);
    }
    printf("\n");

    var table = hash_table_new(char *, int, string_hash, string_equals,
                               &default_allocator);

    result = hash_table_insert(&table, "One", 1);
    assert(result);
    result = hash_table_contains(table, "One");
    assert(result);
    result = hash_table_insert(&table, "Two", 2);
    assert(result);
    result = hash_table_insert(&table, "Three", 3);
    assert(result);
    result = hash_table_insert(&table, "Four", 4);
    assert(result);
    result = hash_table_insert(&table, "Five", 5);
    assert(result);
    result = hash_table_insert(&table, "Six", 6);

    var ht = hash_set_header(table);
    printf("Capacity: %zu\n", ht->capacity);
    printf("Length: %zu\n", ht->length);
    printf("Buckets: %zu\n", hs->buckets_length);
    for (size_t i = 0; i < ht->capacity; i++) {
        struct hash_entry_t *entry = _hash_set_entry_at_impl(ht, i, sizeof(KeyValuePair(char *, int)));
        const size_t next = entry->next;
        if (HASH_ENTRY_IS_NULL(entry)) continue;
        printf("Index: %zu, ", i);
        printf("Hash: %d, ", entry->hash);
        KeyValuePair(char *, int) *kvp = (void *) entry->value;
        printf("Key: %s, ", kvp->key);
        printf("Value: %d, ", kvp->value);
        printf("Bucket: %zu, ", entry->hash % ht->buckets_length);
        printf(next == 0 ? "Next: -\n" : "Next: %zu\n", next - 1);
    }
    printf("\n");

    result = !hash_table_insert(&table, "One", 1);
    assert(result && "Duplicate insert should fail");
    result = !hash_table_insert(&table, "Two", 2);
    assert(result && "Duplicate insert should fail");
    result = !hash_table_insert(&table, "Three", 3);
    assert(result && "Duplicate insert should fail");
    result = !hash_table_insert(&table, "Four", 4);
    assert(result && "Duplicate insert should fail");
    result = !hash_table_insert(&table, "Five", 5);
    assert(result && "Duplicate insert should fail");

    result = hash_table_remove(table, "One");
    assert(result && "Failed to remove");

    ht = hash_set_header(table);
    printf("Capacity: %zu\n", ht->capacity);
    printf("Length: %zu\n", ht->length);
    printf("Buckets: %zu\n", hs->buckets_length);
    for (size_t i = 0; i < ht->capacity; i++) {
        struct hash_entry_t *entry = _hash_set_entry_at_impl(ht, i, sizeof(KeyValuePair(char *, int)));
        const ssize_t next = entry->next;
        if (HASH_ENTRY_IS_NULL(entry)) continue;
        printf("Index: %zu, ", i);
        printf("Hash: %d, ", entry->hash);
        KeyValuePair(char *, int) *kvp = (void *) entry->value;
        printf("Key: %s, ", kvp->key);
        printf("Value: %d, ", kvp->value);
        printf("Bucket: %zu, ", entry->hash % ht->buckets_length);
        printf(next == 0 ? "Next: -\n" : "Next: %zu\n", next - 1);
    }
    printf("\n");

    var map = hash_table_new(int, int, hash_int_ptr, int_ptr_equals, &default_allocator);

    for (int i = 0; i < 5; i++) {
        result = hash_table_insert(&map, i, i * 2);
        assert(result);
        result = hash_table_contains(map, i);
        assert(result);
    }
    result = hash_table_remove(map, 2);
    assert(result);

    ht = hash_set_header(map);
    printf("Capacity: %zu\n", ht->capacity);
    printf("Length: %zu\n", ht->length);
    printf("Buckets: %zu\n", ht->buckets_length);
    for (size_t i = 0; i < ht->capacity; i++) {
        struct hash_entry_t *entry = _hash_set_entry_at_impl(ht, i, sizeof(KeyValuePair(int, int)));
        if (HASH_ENTRY_IS_NULL(entry)) continue;
        const size_t next = entry->next;
        printf("Index: %zu, ", i);
        printf("Hash: %d, ", entry->hash);
        KeyValuePair(int, int) *kvp = (void *) entry->value;
        printf("Key: %d, ", kvp->key);
        printf("Value: %d, ", kvp->value);
        printf("Bucket: %zu, ", entry->hash % ht->buckets_length);
        printf(next == 0 ? "Next: -\n" : "Next: %zu\n", next - 1);
    }
    printf("\n");

    for (int i = 0; i < hash_set_length(map); i++) {
        var kvp = hash_table_at_index(map, i);
        printf("Key: %d, ", kvp.key);
        printf("Key: %d, ", kvp.value);
        printf("\n");
    }

    printf("%d * 2 = %d\n", 1, hash_table_at(map, 1));
    printf("%d * 2 = %d\n", 3, hash_table_at(map, 3));

    struct string_view {
        size_t length;
        char *data;
    };

    struct socket_t {
        int sock_fd;
        int last_error;
        struct socket_address *addr;
    };

    struct user_t {
        struct string_view username;
        struct socket_t socket;
    };

    var users = hash_table_new(char*, struct user_t, string_hash, string_equals, &default_allocator);

    struct user_t user_1 = {
        .username = (struct string_view){
            .data = "John",
            .length = sizeof "John" - 1,
        },
        .socket = (struct socket_t){
            .sock_fd = 1,
            .last_error = 0,
            .addr = NULL,
        },
    };

    struct user_t user_2 = {
        .username = (struct string_view){
            .data = "Jane",
            .length = sizeof "Jane" - 1,
        },
        .socket = (struct socket_t){
            .sock_fd = 2,
            .last_error = 0,
            .addr = NULL,
        },
    };

    hash_table_insert(&users, user_1.username.data, user_1);
    hash_table_insert(&users, user_2.username.data, user_2);

    for (int i = 0; i < hash_table_capacity(users); i++) {
        if (i >= hash_table_length(users)) { break; }
        const var entry = hash_set_entry(users, i);
        if (HASH_ENTRY_IS_NULL(entry)) { continue; }
        const var kvp = *(typeof(users)) entry->value;

        const struct user_t user = kvp.value;
        printf("Username: %s\n", user.username.data);
    }

    return 0;
}
