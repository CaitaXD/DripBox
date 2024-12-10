#ifndef ARRAY_H
#define ARRAY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <common.h>

struct array {
    size_t length;
    uint8_t data[];
};

#define array(T) T*
#define array_header(array__) (container_of(array__, struct array, data))
#define array_length(array__) (array_header(array__)->length)
#define array_new(type__, length__, allocator__) ((type__*)array_new_(sizeof(type__), length__, allocator__)->data)
#define array_clear(array__) memset(array__, 0, array_length(array__) * sizeof(array__[0]))
#define array_contains(array__, element__, equals__) array_contains_impl(array__, sizeof(*array__), &(element__), (equals__))

#define array_stack(type__, length__, ...) \
    ({\
        struct { size_t length; type__ data[(length__)]; } _array;\
       _array.length = (length__);\
       _array;\
    }).data
#define array_set_lazy_eval(array__, offset, ...)\
({\
    var _array = (array__);\
    size_t _i = (offset);\
    IF(HAS_ARGS(__VA_ARGS__))\
    (\
        _array[_i++] = HEAD(__VA_ARGS__);\
        DEFER2(array_set_recursive)()(_array, _i, TAIL(__VA_ARGS__))\
    );\
})
#define array_set_recursive() array_set_lazy_eval
#define array_set(array__, offset, ...) EVAL(array_set_lazy_eval(array__, offset, __VA_ARGS__))

#define array_set_difference(a__, b__, equals__, allocator__)\
((typeof(*a__)*)({\
    var _a = (a__);\
    var _b = (b__);\
    typedef typeof(*_a) A;\
    typedef typeof(*_b) B;\
    static_assert(sizeof(A) == sizeof(B), "Arrays must have the same element size");\
    size_t _len_a = array_length(_a);\
    size_t _len_b = array_length(_b);\
    size_t _capacity = max(_len_a, _len_b);\
    struct array *_out = array_new_(sizeof(A), _capacity, (allocator__));\
    array_set_difference_(sizeof(A), _len_a, _a, _len_b, _b, &_out->length, _out->data, (equals__));\
    _out;\
})->data)

static struct array *array_new_(const size_t element_size, const size_t lenght, struct allocator *allocator) {
    struct array *array = allocator_alloc(allocator, sizeof(struct array) + element_size * lenght);
    array->length = lenght;
    return array;
}

static void *array_set_difference_(size_t element_size,
                                   size_t len_a, const void *array_a,
                                   size_t len_b, const void *array_b,
                                   size_t *out_len, void* out_array,
                                   bool (*equals)(const void *a, const void *b));

static bool array_contains_(const size_t element_size,
                                const size_t len, const void *array,
                                const void *element,
                                bool (*equals)(const void *a, const void *b)) {
    for (size_t i = 0; i < len; i++) {
        const void *element_a = array + i * element_size;
        if (equals(element_a, element)) {
            return true;
        }
    }
    return false;
}

void *array_set_difference_(const size_t element_size,
                            const size_t len_a, const void *array_a,
                            const size_t len_b, const void *array_b,
                            size_t *out_len, void* out_array,
                            bool (*equals)(const void *a, const void *b)) {
    size_t len_out = 0;
    for (size_t i = 0; i < len_a; i++) {
        const void *element_a = array_a + i * element_size;
        const bool belongs_to_b = array_contains_(element_size, len_b, array_b, element_a, equals);
        const bool belongs_to_out = array_contains_(element_size, len_out, out_array, element_a, equals);
        if (!belongs_to_b && !belongs_to_out) {
            memcpy(out_array + len_out * element_size, element_a, element_size);
            len_out += 1;
        }
    }
    *out_len = len_out;
    return out_array;
}

#endif //ARRAY_H
