#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <common.h>

#ifndef ALLOCATOR_API
#define ALLOCATOR_API static inline
#endif

// Allocator API
#ifdef USING_ALLOCATOR

#define alloc allocator_alloc
#define dealloc allocator_dealloc
#define copy allocator_copy
#define using_arena using_allocator_arena
#define arena allocator_arena
#define temp_arena allocator_temp_arena

#endif // USING_ALLOCATOR

struct allocator {
    void * (*alloc)(struct allocator *allocator, size_t size);

    void (*dealloc)(struct allocator *allocator, void *ptr);
};

ALLOCATOR_API void *allocator_alloc(struct allocator *allocator, size_t size);

ALLOCATOR_API void allocator_dealloc(struct allocator *allocator, void *ptr);

ALLOCATOR_API void *allocator_copy(struct allocator *allocator, const void *ptr, size_t size);

#define new2(allocator__, type__, cliteral__)\
    ({\
        var _allocator = (allocator__);\
        size_t _size = sizeof(type__);\
        var _cliteral = (cliteral__);\
        void *_mem = allocator_alloc(_allocator, _size);\
        (type__*)memcpy(_mem, &_cliteral, _size);\
    })

#define new1(allocator__, type__)\
    ({\
        var _allocator = (allocator__);\
        size_t _size = sizeof(type__);\
        (type__*)allocator_alloc(_allocator, _size);\
    })

#define new(...) MACRO_SELECT3(__VA_ARGS__, new2, new1, new1)(__VA_ARGS__)

#ifndef mallocator

extern void *malloc(size_t size);

extern void free(void *ptr);

static void *allocator_default_alloc(const struct allocator *allocator [[maybe_unused]], const size_t size) {
    return malloc(size);
}

static void allocator_default_dealloc(const struct allocator *allocator [[maybe_unused]], void *ptr) {
    free(ptr);
}

static struct allocator mallocator = {
    .alloc = (void*)allocator_default_alloc,
    .dealloc = (void*)allocator_default_dealloc
};

#else
extern struct allocator_t mallocator;
#endif

[[maybe_unused]]
static struct allocator dummy_allocator = {
    .alloc = NULL,
    .dealloc = NULL
};

// Arena Allocator API
#ifdef USING_ALLOCATOR

#define arena_t allocator_arena_t
#define arena_init allocator_arena_init
#define arena_alloc allocator_arena_alloc
#define arena_dealloc allocator_arena_dealloc
#define arena_clear allocator_arena_clear
#define stack_arena allocator_stack_arena
#define static_arena allocator_static_arena

#endif // USING_ALLOCATOR

struct allocator_arena {
    struct allocator allocator;
    uintptr_t min_address;
    uintptr_t max_address;
    uintptr_t current_address;
};

ALLOCATOR_API struct allocator_arena allocator_arena_init(void *address, size_t bytes);

ALLOCATOR_API void *allocator_arena_alloc(struct allocator_arena *allocator, size_t size);

ALLOCATOR_API void *allocator_arena_alloc_aligned(struct allocator_arena *allocator, size_t size, size_t alignment);

ALLOCATOR_API void allocator_arena_dealloc(const struct allocator_arena *allocator, const void *ptr);

ALLOCATOR_API void allocator_arena_clear(struct allocator_arena *allocator);

ALLOCATOR_API struct allocator_arena allocator_arena_detach_frame(struct allocator_arena *arena, ssize_t size);

ALLOCATOR_API void *allocator_arena_detach_bytes(struct allocator_arena *arena, ssize_t size);

#define allocator_stack_arena(size__) allocator_arena_init(STACK_BUFFER(size__), size__)

#define allocator_static_arena(size__) \
    ({\
        static uint8_t _arena_array[(size__)];\
        allocator_arena_init(_arena_array, sizeof _arena_array);\
    })

#define allocator_malloc_arena(size__) allocator_arena_init(malloc(size__), (size__))

#define allocator_arena_new(allocator__, size__) \
    ({\
        var _allocator = (allocator__);\
        size_t _size = (size__);\
        void *_mem = allocator_alloc(_allocator, _size);\
        allocator_arena_init(_mem, _size);\
    })

// Allocator Implementation

void *allocator_alloc(struct allocator *allocator, const size_t size) {
    if (allocator == NULL || allocator->alloc == NULL) {
        return NULL;
    }
    return allocator->alloc(allocator, size);
}

void allocator_dealloc(struct allocator *allocator, void *ptr) {
    if (allocator == NULL || allocator->dealloc == NULL) {
        return;
    }
    allocator->dealloc(allocator, ptr);
}

void *allocator_copy(struct allocator *allocator, const void *ptr, const size_t size) {
    void *copy = allocator_alloc(allocator, size);
    assert(copy != NULL && "Buy more RAM");
    memcpy(copy, ptr, size);
    return copy;
}

// Arena Allocator Implementation

struct allocator_arena allocator_arena_init(void *address, const size_t bytes) {
    return (struct allocator_arena){
        .allocator = {
            .alloc = (typeof_member(struct allocator, alloc)) allocator_arena_alloc,
            .dealloc = (typeof_member(struct allocator, dealloc)) allocator_arena_dealloc
        },
        .current_address = (uintptr_t) address,
        .min_address = (uintptr_t) address,
        .max_address = (uintptr_t) address + bytes,
    };
}

void *allocator_arena_alloc(struct allocator_arena *allocator, const size_t size) {
    assert(allocator != NULL && "Allocator is not initialized");
    if (allocator->current_address + size > allocator->max_address) {
        return NULL;
    }
    const uintptr_t ptr = allocator->current_address;
    allocator->current_address += size;
    return (void *) ptr;
}

/// Detaches a number of bytes from the arena
/// @param arena: struct allocator_arena* -> The arena to detach from
/// @param size: ssize_t -> The size of the bytes to detach, if negative will detach from the end of the arena
/// @return The detached bytes
void *allocator_arena_detach_bytes(struct allocator_arena *arena, const ssize_t size) {
    if (size < 0) {
        assert((arena->max_address - arena->current_address) >= (-size) && "Arena is too small");
        arena->max_address += size;
        void *detached = (void*)arena->max_address;
        return detached;
    }
    assert((arena->current_address + size) <= arena->max_address && "Arena is too small");
    void *detached = (void*)arena->current_address;
    arena->min_address = arena->current_address += size;
    return detached;
}

void *allocator_arena_alloc_aligned(struct allocator_arena *allocator, const size_t size, const size_t alignment) {
    assert(allocator != NULL && "Allocator is not initialized");
    if (allocator->current_address + size > allocator->max_address) {
        return NULL;
    }
    const uintptr_t ptr = allocator->current_address;
    const uintptr_t aligned_ptr = aligned_to(ptr, alignment);
    allocator->current_address = aligned_ptr + size;
    return (void *) aligned_ptr;
}

void allocator_arena_dealloc(const struct allocator_arena *allocator, const void *ptr) {
    assert(allocator != NULL && "Arena allocator was not initialized");
    const intptr_t max = allocator->max_address;
    const intptr_t min = allocator->min_address;
    const intptr_t address = (intptr_t) ptr;
    assert(address <= max && address >= min && "Pointer is out of the reserved stack region");
}

#define using_allocator_arena(arena__)\
    scope(var _checkpoint = ((arena__)->current_address), ((arena__)->current_address = _checkpoint))

#define using_allocator_temp_arena using_allocator_arena(allocator_temp_arena())

void allocator_arena_clear(struct allocator_arena *allocator) {
    allocator->current_address = allocator->min_address;
}

/// Detaches a frame from the arena
/// @param arena: struct allocator_arena* -> The arena to detach from
/// @param size: ssize_t -> The size of the frame to detach, if negative will detach from the end of the arena
/// @return The detached arena
struct allocator_arena allocator_arena_detach_frame(struct allocator_arena *arena, const ssize_t size) {
    if (size < 0) {
        assert((arena->max_address - arena->current_address) >= (-size) && "Arena is too small");
        arena->max_address += size;
        const struct allocator_arena detached = {
            .allocator = arena->allocator,
            .current_address = arena->max_address + size,
            .min_address = arena->max_address + size,
            .max_address = arena->max_address,
        };
        return detached;
    }

    assert((arena->current_address + size) <= arena->max_address && "Arena is too small");
    arena->current_address += size;
    const struct allocator_arena detached = {
        .allocator = arena->allocator,
        .current_address = arena->current_address,
        .min_address = arena->current_address,
        .max_address = arena->max_address,
    };
    return detached;
}

enum { TLS_TMP_ARENA_SIZE = 16 << 10 };
_Thread_local static uint8_t _tls_tmp_arena_buffer[TLS_TMP_ARENA_SIZE] = {};
_Thread_local static struct allocator_arena _tls_tmp_arena;

static void* allocator_arena_ring_alloc(struct allocator_arena *allocator, const size_t size) {
    assert(allocator != NULL && "Allocator is not initialized");
    if (allocator->current_address + size > allocator->max_address) {
        allocator->current_address = allocator->min_address;
        const uintptr_t ptr = allocator->current_address;
        allocator->current_address += size;
        return (void *) ptr;
    }
    const uintptr_t ptr = allocator->current_address;
    allocator->current_address += size;
    return (void *) ptr;
}

static struct allocator_arena* allocator_temp_arena() {
    if (_tls_tmp_arena.current_address == _tls_tmp_arena.min_address) {
        _tls_tmp_arena = (struct allocator_arena) {
            .allocator = {
                .alloc = (void*) allocator_arena_ring_alloc,
                .dealloc = (void*) allocator_arena_dealloc
            },
            .current_address = (uintptr_t) _tls_tmp_arena_buffer,
            .min_address = (uintptr_t) _tls_tmp_arena_buffer,
            .max_address = (uintptr_t) _tls_tmp_arena_buffer + TLS_TMP_ARENA_SIZE,
        };
    }
    return &_tls_tmp_arena;
}


#endif //ALLOCATOR_H
