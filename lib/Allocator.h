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

#endif // USING_ALLOCATOR

struct allocator {
    void * (*alloc)(const struct allocator *allocator, size_t size);

    void (*dealloc)(const struct allocator *allocator, void *ptr);
};

ALLOCATOR_API void *allocator_alloc(const struct allocator *allocator, size_t size);

ALLOCATOR_API void allocator_dealloc(const struct allocator *allocator, void *ptr);

ALLOCATOR_API void *allocator_copy(const struct allocator *allocator, const void *ptr, size_t size);

#define new(allocator__, type__) (type__*)alloc(allocator__, sizeof(type__))

#ifndef DEFAULT_ALLOCATOR

extern void *malloc(size_t size);

extern void free(void *ptr);

static void *allocator_default_alloc(const struct allocator *allocator [[maybe_unused]], const size_t size) {
    return malloc(size);
}

static void allocator_default_dealloc(const struct allocator *allocator [[maybe_unused]], void *ptr) {
    free(ptr);
}

static struct allocator default_allocator = {
    .alloc = allocator_default_alloc,
    .dealloc = allocator_default_dealloc
};

#else
extern struct allocator_t default_allocator;
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

ALLOCATOR_API void allocator_arena_dealloc(const struct allocator_arena *allocator, const void *ptr);

ALLOCATOR_API void allocator_arena_clear(struct allocator_arena *allocator);

#define allocator_stack_arena(size__) allocator_arena_init(STACK_BUFFER(size__), size__)

#define allocator_static_arena(size__) \
    ({\
        static size_t _arena_size = (size__);\
        static uint8_t _arena_array[_arena_size];\
        allocator_arena_init(_arena_array, _arena_size);\
    })

// Allocator Implementation

void *allocator_alloc(const struct allocator *allocator, const size_t size) {
    if (allocator == NULL || allocator->alloc == NULL) {
        return NULL;
    }
    return allocator->alloc(allocator, size);
}

void allocator_dealloc(const struct allocator *allocator, void *ptr) {
    if (allocator == NULL || allocator->dealloc == NULL) {
        return;
    }
    allocator->dealloc(allocator, ptr);
}

void *allocator_copy(const struct allocator *allocator, const void *ptr, const size_t size) {
    void *copy = allocator_alloc(allocator, size);
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

void allocator_arena_dealloc(const struct allocator_arena *allocator, const void *ptr) {
    assert(allocator != NULL && "Arena allocator was not initialized");
    const intptr_t max = allocator->max_address;
    const intptr_t min = allocator->min_address;
    const intptr_t address = (intptr_t) ptr;
    assert(address <= max && address >= min && "Pointer is out of the reserved stack region");
}

void allocator_arena_clear(struct allocator_arena *allocator) {
    allocator->current_address = allocator->min_address;
}

#endif //ALLOCATOR_H
