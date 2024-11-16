#ifndef STRING_VIEW_H
#define STRING_VIEW_H

#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <linux/limits.h>
#include <Allocator.h>

struct string_view_t {
    size_t length;
    char *data;
};

#define sv_args(sv__) (sv__).length, (sv__).data

struct readonly_span_t {
    size_t length;
    const uint8_t *data;
};

static struct string_view_t sv_skip(const struct string_view_t string, const size_t length) {
    return (struct string_view_t){
        .data = string.data + length,
        .length = string.length - length,
    };
}

static struct string_view_t sv_take(const struct string_view_t string, const size_t length) {
    return (struct string_view_t){
        .data = string.data,
        .length = length,
    };
}

static struct string_view_t sv_substr(const struct string_view_t string, const size_t offset, const size_t length) {
    return (struct string_view_t){
        .data = string.data + offset,
        .length = length,
    };
}

static ssize_t sv_index(const struct string_view_t string, const struct string_view_t needle) {
    if (string.length < needle.length) {
        return -1;
    }
    const char *p = strstr(string.data, needle.data);
    if (p == NULL) {
        return -1;
    }
    const size_t index = p - string.data;
    if (index > string.length) {
        return -1;
    }
    return index;
}

#define ARRAY(N, T) struct { \
    T data[N];\
}

struct sv_pair {
    ARRAY(2, struct string_view_t);
};

static struct sv_pair sv_token(const struct string_view_t string,
                               const struct string_view_t delim) {
    struct sv_pair pair = {};
    const ssize_t index = sv_index(string, delim);
    if (index == -1) {
        pair.data[0] = string;
        pair.data[1] = (struct string_view_t){};
    }
    pair.data[0] = sv_take(string, index);
    pair.data[1] = sv_skip(string, index + 1);
    return pair;
}

static struct string_view_t sv_cstr(char *data) {
    return (struct string_view_t){
        .data = data,
        .length = strlen(data),
    };
}

static char *cstr_sv(const struct string_view_t sv, const struct allocator_t *allocator) {
    char *buffer = allocator_alloc(allocator, sv.length + 1);
    memcpy(buffer, sv.data, sv.length);
    buffer[sv.length] = 0;
    return buffer;
}

static bool sv_split_next(struct string_view_t *sv, const struct string_view_t delim, struct string_view_t *out_part) {
    const ssize_t index = sv_index(*sv, delim);
    if (index == -1) {
        *out_part = *sv;
        return false;
    }
    *out_part = sv_take(*sv, index);
    *sv = sv_skip(*sv, index + 1);
    return true;
}

static ssize_t _sv_path_combine_impl(struct string_view_t *dst,
                                     const struct string_view_t a, const struct string_view_t b) {
    static const char PATH_SEPARATOR = '/';

    memcpy(dst->data, a.data, a.length);
    dst->length = a.length;
    if (a.length > 0 && a.data[a.length - 1] != PATH_SEPARATOR) {
        dst->data[a.length] = PATH_SEPARATOR;
        dst->length += 1;
    }

    memcpy(dst->data + dst->length, b.data, b.length);
    dst->length += b.length;

    if (dst->length > PATH_MAX) {
        dst->length = PATH_MAX;
    }
    return dst->length;
}

struct z_string_t {
    size_t length;
    char *data;
};

static struct string_view_t sv_z(const struct z_string_t string) {
    return (struct string_view_t){
        .data = string.data,
        .length = string.length,
    };
}

static struct z_string_t z_cstr(char *cstr) {
    return (struct z_string_t){
        .data = cstr,
        .length = strlen(cstr),
    };
}

static struct z_string_t z_sv(const struct string_view_t sv, const struct allocator_t *allocator) {
    char *buffer = allocator_alloc(allocator, sv.length + 1);
    memcpy(buffer, sv.data, sv.length);
    buffer[sv.length] = 0;
    return (struct z_string_t){
        .data = buffer,
        .length = sv.length,
    };
}

#define MATCH_AND_CAST_PTR(type__, ptr__) type__*: *((type__*)(ptr__))

#define SV(s__) ((struct string_view_t [1]) \
{ \
    ({\
        var __sptr = &(s__);\
        void* __addr = *(void**)__sptr;\
        void** __addrptr = (void**)__sptr;\
        _Generic(__sptr, \
            /* z string */\
            MATCH_AND_CAST_PTR(struct z_string_t, __sptr), MATCH_AND_CAST_PTR(const struct z_string_t, __sptr), \
            MATCH_AND_CAST_PTR(struct z_string_t*, __sptr), MATCH_AND_CAST_PTR(const struct z_string_t*, __sptr), \
            /* string view */\
            MATCH_AND_CAST_PTR(struct string_view_t, __sptr), MATCH_AND_CAST_PTR(const struct string_view_t, __sptr), \
            MATCH_AND_CAST_PTR(struct string_view_t*, __sptr), MATCH_AND_CAST_PTR(const struct string_view_t*, __sptr), \
            /* default */\
            default: sv_cstr((__addr == *__addrptr) ? (char*)__sptr : *(char**)__sptr) \
        );\
    })\
}[0])

#define path_combine(allocator__, ...) ({\
    const struct allocator_t *__a = (allocator__);\
    const struct string_view_t __sv_args[] = {__VA_ARGS__};\
    const size_t __len = sizeof __sv_args / sizeof __sv_args[0];\
    size_t __total_len = 0;\
    for (size_t i = 0; i < __len; i++) {\
        __total_len += __sv_args[i].length + 1;\
    }\
    __total_len = min(__total_len, PATH_MAX);\
    struct string_view_t __acc = (struct string_view_t){\
        .data = (allocator_alloc(__a, __total_len)),\
        .length = 0,\
    };\
    for (size_t i = 0; i < __len; i++) {\
        _sv_path_combine_impl(&__acc, __acc, __sv_args[i]);\
    }\
    __acc.data[__acc.length] = 0;\
    (struct z_string_t){\
        .data = __acc.data,\
        .length = __acc.length\
    };\
})


#endif //STRING_VIEW_H
