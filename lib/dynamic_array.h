#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <Allocator.h>
#include <common.h>

#ifndef DYNAMIC_ARRAY_API
#    define DYNAMIC_ARRAY_API static inline
#endif

struct dynamic_array_header_t {
    struct allocator *allocator;
    size_t capacity;
    size_t length;
    uint8_t data[];
};

struct queue {
    void **self;
    void (*push)(void* fifo, size_t element_size, void* item);
    bool (*try_pop)(void* fifo, size_t element_size, void* item);
    bool (*try_peek)(void* fifo, size_t element_size, void* item);
};

#define fifo_push(fifo__, value__) ({\
    var _fifo = &(fifo__);\
    _fifo->push(_fifo->self, sizeof value__, &(value__));\
})

#define fifo_try_pop(fifo__, value__) ({\
    var _fifo = &(fifo__);\
    _fifo->try_pop(_fifo->self, sizeof *value__, (value__));\
})

#define fifo_try_peek(fifo__, value__) ({\
    var _fifo = &(fifo__);\
    _fifo->try_peek(_fifo->self, sizeof *value__, (value__));\
})

#define dynamic_array(T) T*
#define dynamic_array_new(T, allocator__) dynamic_array_with_capacity(T, 0, (allocator__))
#define dynamic_array_header(dyn__) container_of(dyn__, struct dynamic_array_header_t, data)
#define dynamic_array_length(dyn__) (dyn__ ? dynamic_array_header(dyn__)->length : 0)

#define dynamic_array_with_capacity(T, capacity__, allocator__) \
    ({\
        var __allocator = (allocator__);\
        size_t __capacity = (capacity__);\
        size_t __element_size = sizeof(T);\
        (T*)dynamic_array_with_capacity_impl(__element_size, __capacity, __allocator)->data;\
    })

#define dynamic_array_push(dyn__, element__) \
    ({\
        var _ptr = (dyn__);\
        size_t __element_size = sizeof **_ptr;\
        if (*_ptr == NULL) {\
            *_ptr = (void*)dynamic_array_with_capacity_impl(__element_size, 2, &mallocator)->data;\
        }\
        var _header = dynamic_array_header(*_ptr);\
        dynamic_array_reserve_impl(&_header, __element_size, _header->length + 1);\
        _header->length += 1;\
        *_ptr = (typeof(*_ptr)) _header->data;\
        (*_ptr)[_header->length - 1] = (element__);\
        void_expression();\
    })

#define dynamic_array_pop(dyn__) \
    ({\
        var _ptr = (dyn__);\
        var _header = dynamic_array_header(*_ptr);\
        _header->length -= 1;\
        (*_ptr)[_header->length];\
    })

#define dynamic_array_insert(dyn__, index__, element__) \
    ({\
        var _ptr = (dyn__);\
        size_t _element_size = sizeof **_ptr;\
        if (*_ptr == NULL) {\
            *_ptr = (void*)dynamic_array_with_capacity_impl(_element_size, 2, &mallocator)->data;\
        }\
        var _header = dynamic_array_header(*_ptr);\
        size_t _index = (index__);\
        dynamic_array_shift_impl(&_header, _element_size, _index);\
        *_ptr = (typeof(*_ptr)) _header->data;\
        (*_ptr)[_index] = (element__);\
        void_expression();\
    })

#define dynamic_array_remove_at(dyn__, index__) \
    ({\
        var _ptr = (dyn__);\
        var _header = dynamic_array_header(*_ptr);\
        size_t __element_size = sizeof **_ptr;\
        size_t __index = (index__);\
        dynamic_array_remove_at_impl(&_header, __index, __element_size);\
        _ptr = (void*)_header->data;\
        void_expression();\
    })

#define dynamic_array_contains(dyn__, element__, equals__) \
    ({\
        bool result = false;\
        var _ptr = &(dyn__);\
        var __data = *_ptr;\
        if (__data == NULL) { goto _dynamic_array_contains_end_return_label; }\
        var _header = dynamic_array_header(__data);\
        size_t __element_size = sizeof *(dyn__);\
        result = dynamic_array_contains_impl(_header, __element_size, &(element__), (equals__));\
        _dynamic_array_contains_end_return_label:\
        result;\
    })

#define dynamic_array_remove(dyn__, element__, equals__) \
    ({\
        bool result = false;\
        var _ptr = (dyn__);\
        if (*_ptr == NULL) { goto _dynamic_array_remove_end_return_label; }\
        var _header = dynamic_array_header(*_ptr);\
        size_t __element_size = sizeof **_ptr;\
        result = dynamic_array_remove_impl(&_header, __element_size, &(element__), (equals__));\
        *_ptr = (void*)_header->data;\
        _dynamic_array_remove_end_return_label:\
        result;\
    })

#define dynamic_array_index_of(dyn__, element__, equals__) \
    ({\
        var _ptr = (dyn__);\
        size_t result = NOT_FOUND;\
        if (_ptr == NULL) { goto _dynamic_array_index_of_end_return_label; }\
        size_t __element_size = sizeof *_ptr;\
        var _header = dynamic_array_header(_ptr);\
        result = dynamic_array_index_of_impl(_header, __element_size, &(element__), (equals__));\
        _dynamic_array_index_of_end_return_label:\
        result;\
    })

DYNAMIC_ARRAY_API void dynamic_array_clear(void *array);

DYNAMIC_ARRAY_API struct queue dynamic_array_fifo(void** array);

void dynamic_array_clear(void *array) {
    if (array == NULL) { return; }
    dynamic_array_header(array)->length = 0;
}

static struct dynamic_array_header_t *dynamic_array_with_capacity_impl(const size_t element_size,
                                                                       const size_t capacity,
                                                                       struct allocator *allocator) {
    struct dynamic_array_header_t *buffer = allocator_alloc(
        allocator,
        sizeof(struct dynamic_array_header_t) + element_size * capacity
    );
    if (buffer == NULL) return NULL;
    buffer->capacity = capacity;
    buffer->allocator = allocator;
    buffer->length = 0;
    return buffer;
}

static void dynamic_array_reserve_impl(struct dynamic_array_header_t **buffer,
                                       const size_t element_size,
                                       const size_t capacity) {
    assert(buffer && "Buffer Pointer can't be null");
    if (*buffer == NULL) {
        *buffer = dynamic_array_with_capacity_impl(element_size, capacity, &mallocator);
        return;
    }
    if ((*buffer)->capacity <= capacity) {
        (*buffer)->capacity = max(next_power_of_two(capacity), 2);
        struct dynamic_array_header_t *new_buffer = dynamic_array_with_capacity_impl(
            element_size,
            (*buffer)->capacity << 1,
            (*buffer)->allocator
        );
        assert(new_buffer != NULL && "Allocation failed, Buy more RAM");
        new_buffer->length = (*buffer)->length;
        memcpy(new_buffer->data, (*buffer)->data, element_size * (*buffer)->length);
        allocator_dealloc((*buffer)->allocator, *buffer);
        *buffer = new_buffer;
    }
}

static void *dynamic_array_shift_impl(struct dynamic_array_header_t **buffer,
                                      const size_t element_size,
                                      const size_t index) {
    assert(buffer != NULL);
    if (*buffer == NULL) {
        *buffer = dynamic_array_with_capacity_impl(element_size, index + 1, &mallocator);
    }
    assert(index <= (*buffer)->length && "Index out of bounds");

    dynamic_array_reserve_impl(buffer, element_size, (*buffer)->length + 1);
    struct dynamic_array_header_t *buffer_ = *buffer;
    buffer_->length += 1;
    const size_t byte_size = element_size * buffer_->length;
    const size_t src_offset = element_size * index;
    const size_t dst_offset = element_size * (index + 1);
    memmove(
        buffer_->data + dst_offset,
        buffer_->data + src_offset,
        byte_size - src_offset
    );
    void *data = buffer_->data + src_offset;
    return data;
}

static void dynamic_array_remove_at_impl(struct dynamic_array_header_t **buffer,
                                         const size_t index,
                                         const size_t element_size) {
    assert(buffer != NULL);
    assert(*buffer != NULL || index == 0);
    assert(index < (*buffer)->length && "Index out of bounds");

    struct dynamic_array_header_t *buffer_ = *buffer;
    const size_t byte_size = element_size * buffer_->length;
    const size_t src_offset = element_size * (index + 1);
    const size_t dst_offset = element_size * index;
    memmove(
        buffer_->data + dst_offset,
        buffer_->data + src_offset,
        byte_size - src_offset
    );
    buffer_->length -= 1;
}

static bool dynamic_array_contains_impl(const struct dynamic_array_header_t *buffer,
                                        const size_t element_size,
                                        const void *element,
                                        bool (*equals)(const void *a, const void *b)) {
    if (buffer == NULL) { return false; }
    for (size_t i = 0; i < buffer->length; i++) {
        const void *element_a = buffer->data + i * element_size;
        if (equals(element_a, element)) {
            return true;
        }
    }
    return false;
}

#define NOT_FOUND ((size_t)-1)

static size_t dynamic_array_index_of_impl(const struct dynamic_array_header_t *buffer,
                                          const size_t element_size,
                                          const void *element,
                                          bool (*equals)(const void *a, const void *b)) {
    if (buffer == NULL) { return NOT_FOUND; }
    for (size_t i = 0; i < buffer->length; i++) {
        const void *element_a = buffer->data + i * element_size;
        if (equals(element_a, element)) {
            return i;
        }
    }
    return NOT_FOUND;
}

static bool dynamic_array_remove_impl(struct dynamic_array_header_t **buffer,
                                      const size_t element_size,
                                      const void *element,
                                      bool (*equals)(const void *a, const void *b)) {
    assert(buffer != NULL);
    if (*buffer == NULL) { return false; }

    for (size_t i = 0; i < (*buffer)->length; i++) {
        const void *element_a = (*buffer)->data + i * element_size;
        if (equals(element_a, element)) {
            dynamic_array_remove_at_impl(buffer, i, element_size);
            return true;
        }
    }
    return false;
}

static void dynamic_array_fifo_push(struct queue* q, const size_t element_size, const void* item) {
    struct element { uint8_t bytes[element_size]; } **array = (void*)q;
    dynamic_array_insert(array, 0, *(struct element*)item);
}

static bool dynamic_array_fifo_try_pop(struct queue* q, const size_t element_size, void* item) {
    struct element { uint8_t bytes[element_size]; } **array = (void*)q;
    const var header = dynamic_array_header(*array);
    if(header->length == 0) return false;

    memcpy(item, &(*array)[header->length - 1], element_size);
    dynamic_array_pop(array);
    return true;
}

static bool dynamic_array_fifo_try_peek(struct queue* q, const size_t element_size, void* item) {
    struct element { uint8_t bytes[element_size]; } **array = (void*)q;
    const var header = dynamic_array_header(*array);
    if(header->length == 0) return false;

    memcpy(item, &(*array)[header->length - 1], element_size);
    return true;
}

struct queue dynamic_array_fifo(void** array) {
    const struct queue fifo = {
        .self = array,
        .push = (void*)dynamic_array_fifo_push,
        .try_pop = (void*)dynamic_array_fifo_try_pop,
        .try_peek = (void*)dynamic_array_fifo_try_peek,
    };
    return fifo;
}

#endif //DYNAMIC_ARRAY_H
