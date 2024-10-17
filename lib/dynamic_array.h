#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <Allocator.h>

#include "common.h"

#ifndef DYNAMIC_ARRAY_API
#    define DYNAMIC_ARRAY_API static inline
#endif

struct dynamic_array_header_t {
    struct allocator_t *allocator;
    size_t capacity;
    size_t length;
    uint8_t data[];
};

#define dynamic_array(T) T*
#define dynamic_array_new(T, allocator__) dynamic_array_with_capacity(T, 0, (allocator__))
#define dynamic_array_header(dyn__) container_of(dyn__, struct dynamic_array_header_t, data)
#define dynamic_array_length(dyn__) dynamic_array_header(dyn__)->length

#define dynamic_array_with_capacity(T, capacity__, allocator__) \
    ({\
        auto_var(__allocator, (allocator__));\
        size_t __capacity = (capacity__);\
        size_t __element_size = sizeof(T);\
        (T*)_dynamic_array_with_capacity_impl(__element_size, __capacity, __allocator)->data;\
    })

#define dynamic_array_push(dyn__, element__) \
    ({\
        auto_var(__data, (dyn__));\
        auto_var(__header, dynamic_array_header(__data));\
        size_t __element_size = sizeof(*(dyn__));\
        __header->length += 1;\
        _dynamic_array_reserve_impl(&__header, __element_size, __header->length);\
        __data[__header->length - 1] = (element__);\
        void_expression();\
    })

#define dynamic_array_pop(dyn__) \
    ({\
        auto_var(__data, (dyn__));\
        auto_var(__header, dynamic_array_header(__data));\
        size_t __element_size = sizeof(*(dyn__));\
        __header->length -= 1;\
        __data[__header->length];\
    })

static struct dynamic_array_header_t *_dynamic_array_with_capacity_impl(
    const size_t element_size, const size_t capacity,
    struct allocator_t *allocator) {
    struct dynamic_array_header_t *buffer = alloc(
        allocator, sizeof(struct dynamic_array_header_t) + element_size * capacity);
    if (buffer == NULL) return NULL;
    buffer->capacity = capacity;
    buffer->allocator = allocator;
    buffer->length = 0;
    return buffer;
}

static void _dynamic_array_reserve_impl(struct dynamic_array_header_t **buffer, const size_t element_size,
                                        const size_t capacity) {
    assert(buffer && "Buffer Pointer can't be null");
    struct dynamic_array_header_t *buffer_ = *buffer;
    if (buffer_ == NULL) {
        *buffer = _dynamic_array_with_capacity_impl(element_size, capacity, NULL);
        return;
    }
    if (buffer_->capacity <= capacity) {
        buffer_->capacity = max(capacity, (size_t)4);
        struct allocator_t *allocator = (*buffer)->allocator;
        struct dynamic_array_header_t *new_buffer = _dynamic_array_with_capacity_impl(
            element_size, buffer_->capacity << 1, allocator);
        assert(new_buffer != NULL && "Allocation failed, Buy more RAM");
        new_buffer->length = (*buffer)->length;
        memcpy(new_buffer->data, (*buffer)->data, element_size * (*buffer)->length);
        dealloc(buffer_->allocator, *buffer);
        *buffer = new_buffer;
    }
}

#endif //DYNAMIC_ARRAY_H
