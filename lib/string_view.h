#ifndef STRING_VIEW_H
#define STRING_VIEW_H

#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <linux/limits.h>
#include <Allocator.h>

struct string_view {
    size_t length;
    char *data;
};

#define sv_deconstruct(sv__) (sv__).length, (sv__).data
#define sv_fmt "%.*s"

struct readonly_span_t {
    size_t length;
    const uint8_t *data;
};

static struct string_view sv_skip(const struct string_view string, const size_t length) {
    return (struct string_view){
        .data = string.data + length,
        .length = string.length - length,
    };
}

static struct string_view sv_take(const struct string_view string, const size_t length) {
    return (struct string_view){
        .data = string.data,
        .length = length,
    };
}

static struct string_view sv_substr(const struct string_view string, const size_t offset, const size_t length) {
    return (struct string_view){
        .data = string.data + offset,
        .length = length,
    };
}

static ssize_t sv_index(const struct string_view string, const struct string_view needle) {
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
    ARRAY(2, struct string_view);
};

static struct sv_pair sv_token(const struct string_view string,
                               const struct string_view delim) {
    struct sv_pair pair = {};
    const ssize_t index = sv_index(string, delim);
    if (index == -1) {
        pair.data[0] = string;
        pair.data[1] = (struct string_view){};
    }
    pair.data[0] = sv_take(string, index);
    pair.data[1] = sv_skip(string, index + 1);
    return pair;
}

static struct string_view sv_new(const size_t len, char data[len]) {
    return (struct string_view){
        .data = data,
        .length = len,
    };
}

static struct string_view sv_cstr(char *data) {
    return (struct string_view){
        .data = data,
        .length = strlen(data),
    };
}

static char *cstr_sv(const struct string_view sv, const struct allocator *allocator) {
    char *buffer = allocator_alloc(allocator, sv.length + 1);
    memcpy(buffer, sv.data, sv.length);
    buffer[sv.length] = 0;
    return buffer;
}

static bool sv_split_next(struct string_view *sv, const struct string_view delim, struct string_view *out_part) {
    const ssize_t index = sv_index(*sv, delim);
    if (index == -1) {
        *out_part = *sv;
        return false;
    }
    *out_part = sv_take(*sv, index);
    *sv = sv_skip(*sv, index + 1);
    return true;
}

#include <stdarg.h>

static struct string_view sv_empty = { .data = "" };

#define sv_stack(size__) sv_new(size__, alloca(size__))

#define sv_static(size__) ({\
    static char __sv_static_buffer[size__];\
    sv_new(size__, __sv_static_buffer);\
})

#define sv_printf(sv__, format__, ...) (sv_prtinf)(SV(sv__), format__, ##__VA_ARGS__);

static struct string_view (sv_prtinf)(const struct string_view sv, const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int n = vsnprintf(sv.data, sv.length, format, args);
    va_end(args);
    if (n < 0) { return sv_empty; }
    return sv_take(sv, n);
}

static const char PATH_SEPARATOR = '/';

static ssize_t sv_path_combine_impl(struct string_view *dst, const struct string_view a, const struct string_view b) {
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

struct z_string {
    size_t length;
    char *data;
};

static struct string_view sv_z(const struct z_string string) {
    return (struct string_view){
        .data = string.data,
        .length = string.length,
    };
}

static struct z_string z_cstr(char *cstr) {
    return (struct z_string){
        .data = cstr,
        .length = strlen(cstr),
    };
}

static struct z_string z_sv(const struct string_view sv, const struct allocator *allocator) {
    char *buffer = allocator_alloc(allocator, sv.length + 1);
    memcpy(buffer, sv.data, sv.length);
    buffer[sv.length] = 0;
    return (struct z_string){
        .data = buffer,
        .length = sv.length,
    };
}

static bool (sv_equals)(const struct string_view a, const struct string_view b) {
    return a.length == b.length && memcmp(a.data, b.data, a.length) == 0;
}

#define sv_equals(a__, b__) (sv_equals)(SV(a__), SV(b__))

#define MATCH_PTR_CAST_RETURN(type__, ptr__) \
    type__*: *((type__*)(ptr__)), \
    type__ *const: *((type__ *const)(ptr__)), \
    const type__*: *((const type__*)(ptr__)), \
    const type__ *const: *((const type__ *const)(ptr__))

#define MATCH_PTR(type__, do__) \
    type__*: (do__), \
    type__ *const: (do__), \
    const type__*: (do__), \
    const type__ *const: (do__)

#define SV(str__) ((struct string_view [1]) \
{ \
    ({\
        var _str = (str__);\
        var _ptr = &_str;\
        var _ret = _Generic(_ptr, \
            MATCH_PTR_CAST_RETURN(struct string_view, _ptr),\
            MATCH_PTR(struct z_string, sv_z(*(struct z_string *)_ptr)),\
            MATCH_PTR(char*, sv_cstr(*(char**)_ptr))\
        );\
        _ret;\
    })\
}[0])

#define path_combine(...) ({\
    const struct string_view __sv_args[] = { MAP(SV, __VA_ARGS__) };\
    const size_t __len = sizeof __sv_args / sizeof __sv_args[0];\
    static char __path_buffer[PATH_MAX] = {};\
    memset(__path_buffer, 0, sizeof __path_buffer);\
    struct string_view __acc = (struct string_view){\
        .data = __path_buffer,\
        .length = 0,\
    };\
    for (size_t i = 0; i < __len; i++) {\
        sv_path_combine_impl(&__acc, __acc, __sv_args[i]);\
    }\
    __acc.data[__acc.length] = 0;\
    (struct z_string){ .data = __acc.data, .length = __acc.length };\
})


#endif //STRING_VIEW_H
