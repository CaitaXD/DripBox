#ifndef HASH_SET_H
#define HASH_SET_H

#include <PREPROCESSOR_CALCULUS.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "Allocator.h"
#include "common.h"

#ifndef HASH_SET_API
#    define HASH_SET_API static inline
#endif

#define hash_set(T) T*
#define hash_set_entry_header(set__) container_of((set__), struct hash_entry_t, value)
#define hash_set_header(set__) container_of(hash_set_entry_header((set__)), struct hash_set_t, entries)
#define hash_set_length(set__) ((set__) ? hash_set_header((set__))->length : 0)
#define hash_set_capacity(set__) ((set__) ? hash_set_header((set__))->capacity : 0)
#define hash_set_allocator(set__) ((set__) ? hash_set_header((set__))->allocator : NULL)
#define hash_set_entry(set__, index__) (hash_set_entry_at_impl(hash_set_header(set__), (index__), sizeof(*(set__))))
#define hash_set_try_entry(set__, index__, out__) ({\
    bool _result;\
    var _set = (set__);\
    var _i = (index__);\
    var _out = (out__);\
    var _entry = hash_set_entry(_set, _i);\
    if (HASH_ENTRY_IS_NULL(_entry)) {\
        _result = false;\
        goto LINE_VAR(_hash_set_try_entry_return_label);\
    }\
    *_out = _entry;\
    _result = true;\
    LINE_VAR(_hash_set_try_entry_return_label):\
    _result;\
})

#define hash_table(TKey, TValue) key_value_pair(TKey, TValue)*
#define hash_table_length(table__) hash_set_length((table__))
#define hash_table_capacity(table__) hash_set_capacity((table__))
#define hash_table_header(table__) hash_set_header((table__))
#define hash_table_allocator(table__) hash_set_allocator((table__))
#define hash_table_entry_header(table__) hash_set_entry_header((table__))
#define hash_table_entry(table__, index__) \
    (hash_set_entry_at_impl(hash_table_header(table__), (index__), sizeof(*(table__))))
#define hash_table_at_index(table__, index__) hash_set_at_index(table__, (index__))

struct hash_entry_t {
    size_t next; // 1-based index, collisions linked list
    int32_t hash;
    uint8_t value[];
};

struct hash_set_t {
    struct allocator *allocator;
    size_t length;
    size_t capacity;
    int32_t (*hash)(const void *element);
    bool (*equals)(const void *a, const void *b);
    size_t buckets_length;
    int32_t *buckets;
    struct hash_entry_t entries[];
};


enum { HASH_SET_MIN_BUCKETS = 5 };

enum { HASH_SET_MIN_CAPACITY = 4 };

#define hash_set_with_capacity(T, capacity__, hash__, equals__, allocator__) \
    ((T*)hash_set_with_capacity_impl(\
        sizeof(T),\
        (capacity__),\
        (hash__),\
        (equals__),\
        (allocator__)\
    )->entries->value)

#define hash_set_new(T, hash__, equals__, allocator__) \
    ((T*)hash_set_with_capacity_impl(\
        sizeof(T),\
        0,\
        (hash__),\
        (equals__),\
        (allocator__)\
    )->entries->value)

#define hash_table_new(TKey, TValue, hash__, equals__, allocator__) \
    (hash_table(TKey, TValue)) ((void*) hash_set_with_capacity_impl(\
        sizeof(key_value_pair(TKey, TValue)),\
        0,\
        (hash__),\
        (equals__),\
        (allocator__)\
    )->entries->value)

#define hash_table_with_capacity(TKey, TValue, capacity__, hash__, equals__, allocator__) \
    ((void *)hash_set_with_capacity_impl(\
        sizeof(key_value_pair(TKey, TValue)),\
        (capacity__),\
        (hash__),\
        (equals__),\
        (allocator__)\
    )->entries->value)

#define hash_set_insert(set__, element__) \
    ({\
        var __data_ptr = &(set__);\
        var __data = *__data_ptr;\
        var __header = hash_set_header(__data);\
        var __element = (element__);\
        size_t __element_size = sizeof __data;\
        bool result = hash_set_insert_impl(&__header, __element_size, &__element, 0, NULL);\
        __data = (void*)__header->entries->value;\
        *__data_ptr = __data;\
        result;\
    })

#define hash_set_remove(set__, element__) \
    ({\
        var __data_ptr = &(set__);\
        var __data = *__data_ptr;\
        var __header = hash_set_header(__data);\
        var __element = (element__);\
        size_t __element_size = sizeof __data;\
        bool result = hash_set_remove_impl(&__header, __element_size, &__element);\
        __data = (void*)__header->entries->value;\
        *__data_ptr = __data;\
        result;\
    })

#define hash_set_contains(set__, element__) \
    ({\
        var __data_ptr = &(set__);\
        var __data = *__data_ptr;\
        var __header = hash_set_header(__data);\
        var __element = (element__);\
        size_t __element_size = sizeof __data;\
        bool result = hash_set_get_location_impl(__header, __element_size, &__element, NULL, NULL);\
        result;\
    })

#define hash_set_at_index(set__, index__)\
    ({\
        var __entry = hash_set_entry((set__), (index__));\
        var __value = __entry->value;\
        *((typeof(set__)) __value);\
    })

#define key_value_pair(TKey, TValue) struct { TKey key; TValue value; }
//__attribute__((aligned(sizeof(max_align_t))))


#define hash_table_insert(table__, key__, value__) \
    ({\
        var __data_ptr = (table__);\
        var __data = *__data_ptr;\
        var __header = hash_set_header(__data);\
        var _kvp = (key_value_pair(typeof(key__), typeof(value__))) {\
            .key = (key__),\
            .value = (value__),\
        };\
        size_t _element_size = sizeof(key_value_pair(typeof(key__), typeof(value__)));\
        bool result = hash_set_insert_impl(&__header, _element_size, &_kvp);\
        __data = (void*)__header->entries->value;\
        *__data_ptr = __data;\
        result;\
    })

#define hash_table_update(table__, key__, value__) \
    ({\
        var __data_ptr = (table__);\
        var __data = *__data_ptr;\
        var __header = hash_set_header(__data);\
        var _kvp = (key_value_pair(typeof(key__), typeof(value__))) {\
            .key = (key__),\
            .value = (value__),\
        };\
        size_t _element_size = sizeof(key_value_pair(typeof(key__), typeof(value__)));\
        hash_table_update_impl(&__header, _element_size, &_kvp);\
        __data = (void*)__header->entries->value;\
        *__data_ptr = __data;\
        void_expression();\
    })

#define hash_table_contains(table__, key__) \
    ({\
        bool result = false;\
        if (table__ == NULL) goto _hash_table_contains_return_label;\
        var __table = (table__);\
        var __data_ptr = &(table__);\
        var __data = *__data_ptr;\
        var __header = hash_set_header(__data);\
        var __key = (key__);\
        size_t __element_size = sizeof *__table;\
        result = hash_set_get_location_impl(__header, __element_size, &__key, NULL, NULL);\
        _hash_table_contains_return_label:\
        result;\
    })

#define hash_table_remove(table__, key__) ({\
    var __table = (table__);\
    var __data_ptr = &(table__);\
    var __data = *__data_ptr;\
    var __header = hash_set_header(__data);\
    var __key = (key__);\
    size_t __element_size = sizeof *__table;\
    bool result = hash_set_remove_impl(&__header, __element_size, &__key);\
    __data = (void*)__header->entries->value;\
    *__data_ptr = __data;\
    result;\
})

#define hash_table_at(table__, key__) ({\
    var __table = (table__);\
    var __key = (key__);\
    size_t __element_size = sizeof __table->value;\
    size_t __key_size = sizeof __key;\
    var __value = hash_table_at_impl(hash_table_header(__table), __key_size, &__key, __element_size);\
    assert(__value && "Key not found");\
    ((typeof(__table)) __value)->value;\
})


// When next is -1, the entry is null
#define HASH_SET_NULL_HANDLE (ssize_t)(-1)
#define HASH_ENTRY_IS_NULL(entry__) (entry__)->next == (ssize_t)(-1)
#define HASH_ENTRY_SET_NULL(entry__) (entry__)->next = (ssize_t)(-1)

static bool hash_set_insert_impl(struct hash_set_t **set,
                                   size_t element_size, const void *element);

static struct hash_entry_t *hash_set_entry_at_impl(const struct hash_set_t *set, size_t index, size_t element_size);

static void hash_set_erase_impl(const struct hash_set_t *set, size_t element_size, size_t offset,
                                size_t count);

static bool hash_set_get_location_impl(const struct hash_set_t *set, size_t element_size, const void *element,
                                       int32_t *hash, int32_t *location);

static struct hash_set_t *hash_set_with_capacity_impl(
    const size_t element_size,
    const size_t capacity,
    int32_t (*hash)(const void *element),
    bool (*equals)(const void *a, const void *b),
    struct allocator *allocator) {
    const size_t entry_size = sizeof(struct hash_entry_t) + element_size;
    const size_t entries_size = capacity * entry_size;
    const size_t alloc_size = sizeof(struct hash_set_t) + entries_size;
    struct hash_set_t *set = allocator_alloc(allocator, alloc_size);
    assert(set && "Buy more RAM");
    memset(set->entries, 0, entries_size);
    hash_set_erase_impl(set, element_size, 0, capacity);

    const size_t buckets_length = max(next_prime(capacity), HASH_SET_MIN_BUCKETS);
    int32_t *buckets = allocator_alloc(allocator, buckets_length * sizeof(int32_t));
    assert(buckets && "Buy more RAM");
    memset(buckets, 0, buckets_length * sizeof(int32_t));

    *set = (struct hash_set_t) {
        .allocator = allocator,
        .capacity = capacity,
        .hash = hash,
        .buckets_length = buckets_length,
        .buckets = buckets,
        .equals = equals,
    };

    return set;
}

static struct hash_entry_t *hash_set_entry_at_impl(const struct hash_set_t *set, const size_t index,
                                                   const size_t element_size) {
    uint8_t *entry_ptr = (void *) set->entries;
    const size_t entry_size = sizeof(struct hash_entry_t) + element_size;
    struct hash_entry_t *entry = (struct hash_entry_t *) (entry_ptr + entry_size * index);
    return entry;
}

static void hash_set_erase_impl(const struct hash_set_t *set, const size_t element_size, const size_t offset,
                                const size_t count) {
    for (size_t i = offset; i < count; i++) {
        HASH_ENTRY_SET_NULL(hash_set_entry_at_impl(set, i, element_size));
    }
}

static void hash_set_grow_buckets_impl(struct hash_set_t *set, const size_t element_size, const size_t hint) {
    const size_t new_buckets_length = next_prime(hint);
    int32_t *new_buckets = allocator_alloc(set->allocator, new_buckets_length * sizeof(int32_t));
    memset(new_buckets, 0, new_buckets_length * sizeof(int32_t));

    const size_t len = set->length;
    allocator_dealloc(set->allocator, set->buckets);
    set->length = 0;
    set->buckets = new_buckets;
    set->buckets_length = new_buckets_length;
    hash_set_erase_impl(set, element_size, 0, set->capacity);

    for (size_t i = 0; i < len; i++) {
        const struct hash_entry_t *entry = hash_set_entry_at_impl(set, i, element_size);
        hash_set_insert_impl(&set, element_size, entry->value);
    }
}

static bool hash_set_get_location_impl(const struct hash_set_t *set, const size_t element_size, const void *element,
                                   int32_t *hash, int32_t *location) {
    const int32_t h = set->hash(element);
    const int32_t b = set->buckets[h % set->buckets_length];
    if (hash) *hash = h;
    if (b == 0) {
        if (location) *location = 0;
        return false;
    }

    int32_t entry_handle = b;

    const struct hash_entry_t *e = hash_set_entry_at_impl(set, entry_handle - 1, element_size);
    if (HASH_ENTRY_IS_NULL(e)) { return false; }

    while (true) {
        if (h == e->hash && set->equals(element, e->value)) {
            if (location) *location = entry_handle;
            return true;
        }
        if (e->next == 0) {
            break;
        }
        entry_handle = e->next;
        e = hash_set_entry_at_impl(set, entry_handle - 1, element_size);
    }

    if (location) *location = 0;
    return false;
}

static void hash_table_insert_location_impl(struct hash_set_t **set,
                                             const size_t element_size, const void *element,
                                             int32_t hash, int32_t location) {
    const size_t kvp_size = element_size;
    const int threshold_a = 1;
    const int threshold_b = 1;
    assert(((double)threshold_a / (double)threshold_b) >= 1 && "Thresholds must be >= 1");

    if (threshold_a * (*set)->length >= threshold_b * (*set)->capacity) {
        const size_t entry_size = sizeof(struct hash_entry_t) + kvp_size;
        const size_t new_capacity = max(HASH_SET_MIN_CAPACITY, (*set)->capacity << 1, (*set)->length);
        const size_t entries_size = new_capacity * entry_size;
        const size_t alloc_size = sizeof(struct hash_set_t) + entries_size;
        struct hash_set_t *new_set = allocator_alloc((*set)->allocator, alloc_size);

        *new_set = (struct hash_set_t){
            .allocator = (*set)->allocator,
            .buckets_length = (*set)->buckets_length,
            .buckets = (*set)->buckets,
            .length = (*set)->length,
            .hash = (*set)->hash,
            .equals = (*set)->equals,
            .capacity = new_capacity,
        };

        hash_set_erase_impl(new_set, kvp_size, 0, new_set->capacity);
        memcpy(new_set->entries, (*set)->entries, entry_size * (*set)->length);

        allocator_dealloc((*set)->allocator, *set);
        *set = new_set;

        const size_t hint = max(HASH_SET_MIN_BUCKETS, (*set)->capacity);
        hash_set_grow_buckets_impl(*set, kvp_size, hint);

        const bool sanity = !hash_set_get_location_impl(*set, kvp_size, element, &hash, &location);
        assert(sanity);
    }

    (*set)->length += 1;
    if (location == 0) {
        location = (*set)->length;
    }

    struct hash_entry_t *entry = hash_set_entry_at_impl(*set, location - 1, kvp_size);
    entry->hash = hash;
    entry->next = 0;
    memcpy(entry->value, element, element_size);

    int32_t *bucket = (*set)->buckets + hash % (*set)->buckets_length;
    if (*bucket == 0) {
        *bucket = location;
    }
    if (location != *bucket) {
        entry = hash_set_entry_at_impl(*set, *bucket - 1, kvp_size);
        while (entry->next > 0) {
            entry = hash_set_entry_at_impl(*set, entry->next - 1, kvp_size);
        }
        entry->next = location;
    }
}

static void hash_table_update_impl(struct hash_set_t **set, const size_t element_size, const void *element) {
    int32_t hash, location = 0;
    if (hash_set_get_location_impl(*set, element_size, element, &hash, &location)) {
        struct hash_entry_t *entry = hash_set_entry_at_impl(*set, location - 1, element_size);
        memcpy(entry->value, element, element_size);
        return;
    }
    hash_table_insert_location_impl(set, element_size, element, hash, location);
}

static bool hash_set_insert_impl(struct hash_set_t **set, const size_t element_size, const void *element) {
    int32_t hash, location = 0;
    if (hash_set_get_location_impl(*set, element_size, element, &hash, &location)) {
        return false;
    }

    hash_table_insert_location_impl(set, element_size, element, hash, location);
    return true;
}

static void *hash_table_at_impl(const struct hash_set_t *set, const size_t key_size, const void *key,
                                 const size_t value_size) {
    int32_t location = 0;
    const size_t kvp_size = key_size + value_size;
    if (!hash_set_get_location_impl(set, kvp_size, key, NULL, &location)) {
        return NULL;
    }
    struct hash_entry_t *entry = hash_set_entry_at_impl(set, location - 1, key_size + value_size);
    if (entry->next == HASH_SET_NULL_HANDLE) {
        return NULL;
    }
    return entry->value;
}

static bool hash_set_remove_impl(struct hash_set_t **set, const size_t element_size, const void *element) {
    int32_t hash, location = 0;
    if (!hash_set_get_location_impl(*set, element_size, element, &hash, &location)) {
        return false;
    }
    struct hash_entry_t *entry = hash_set_entry_at_impl(*set, location - 1, element_size);
    int32_t *bucket = (*set)->buckets + hash % (*set)->buckets_length;
    if (*bucket == location && entry->next == 0) {
        *bucket = 0;
    }
    if (HASH_ENTRY_IS_NULL(entry)) {
        return false;
    }
    if (entry->next == 0) {
        HASH_ENTRY_SET_NULL(entry);
    } else {
        struct hash_entry_t *next_entry = hash_set_entry_at_impl(*set, entry->next - 1, element_size);
        entry->hash = next_entry->hash;
        entry->next = next_entry->next;
        memcpy(entry->value, next_entry->value, element_size);
        HASH_ENTRY_SET_NULL(next_entry);
    }
    (*set)->length -= 1;
    return true;
}

#endif //HASH_SET_H
