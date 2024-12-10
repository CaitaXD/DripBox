#ifndef STRING_VIEW_H
#define STRING_VIEW_H

#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <linux/limits.h>
#include <Allocator.h>
#include <iterator.h>
#include <PREPROCESSOR_CALCULUS.h>

struct string_view {
    size_t length;
    char *data;
};

static bool (sv_equals)(struct string_view a, struct string_view b);

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

static struct sv_pair (sv_token)(const struct string_view string,
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

#define sv_token(string__, delim__) ((sv_token)(SV(string__), SV(delim__))).data

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

static char *cstr_sv(const struct string_view sv, struct allocator *allocator) {
    char *buffer = allocator_alloc(allocator, sv.length + 1);
    memcpy(buffer, sv.data, sv.length);
    buffer[sv.length] = 0;
    return buffer;
}

#define sv_empty ((struct string_view){ .data = "" })
#define sv_null ((struct string_view){ .data = NULL })

static bool (sv_split_next)(struct string_view *sv, const struct string_view delim, struct string_view *out) {
    if (sv->data == NULL) { return false; }

    const ssize_t index = sv_index(*sv, delim);
    if (index == -1) {
        *out = *sv;
        *sv = sv_null;
        return true;
    }
    *out = sv_take(*sv, index);
    *sv = sv_skip(*sv, index + 1);
    return true;
}

#define sv_split_next(sv__, delim__, out__) (sv_split_next)(sv__, SV(delim__), out__)

struct sv_split_iterator {
    bool (*next)(struct sv_split_iterator*);
    struct string_view *(*current)(struct sv_split_iterator*);
    struct string_view sv;
    struct string_view delim;
    struct string_view _current;
};

static bool sv_split_next_iterator_next(struct sv_split_iterator *it) {
    return (sv_split_next)(&it->sv, it->delim, &it->_current);
}

static struct string_view *sv_split_next_iterator_current(struct sv_split_iterator *it) {
    return &it->_current;
}

static struct sv_split_iterator (sv_split)(const struct string_view sv, const struct string_view delim) {
    return (struct sv_split_iterator) {
        .next = sv_split_next_iterator_next,
        .current = sv_split_next_iterator_current,
        .sv = sv,
        .delim = delim,
    };
}

#define sv_split(sv__, delim__) (sv_split)(SV(sv__), SV(delim__))

#include <stdarg.h>

#define sv_stack(size__) sv_new((size__), alloca((size__)))
#define sv_malloc(size__) sv_new((size__), malloc((size__)))
#define sv_temp(size__) sv_new((size__), allocator_arena_alloc(allocator_temp_arena(), (size__)))

#define sv_static(size__) ({\
    static char __sv_static_buffer[size__];\
    sv_new(size__, __sv_static_buffer);\
})

#define sv_printf(sv__, format__, ...) (sv_prtinf)(SV(sv__), format__, ##__VA_ARGS__)

static struct string_view (sv_prtinf)(const struct string_view sv, const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int n = vsnprintf(sv.data, sv.length, format, args);
    va_end(args);
    if (n < 0) { return sv_empty; }
    return sv_take(sv, n);
}

static ssize_t sv_concat2_impl(struct string_view *dst, const struct string_view a, const struct string_view b) {
    memcpy(dst->data, a.data, a.length);
    dst->length = a.length;
    memcpy(dst->data + dst->length, b.data, b.length);
    dst->length += b.length;
    return dst->length;
}

static const char PATH_SEPARATOR = '/';

static ssize_t sv_path_combine2_impl(struct string_view *dst, const struct string_view a, const struct string_view b) {
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

static struct z_string z_sv(const struct string_view sv, struct allocator *allocator) {
    char *buffer = allocator_alloc(allocator, sv.length + 1);
    memcpy(buffer, sv.data, sv.length);
    buffer[sv.length] = 0;
    return (struct z_string){
        .data = buffer,
        .length = sv.length,
    };
}

static ssize_t (sv_ends_with)(const struct string_view str, const struct string_view suffix)
{
    if (suffix.length >  str.length) { return 0; }
    return strncmp(str.data + str.length - suffix.length, suffix.data, suffix.length) == 0;
}

#define sv_ends_with(str__, suffix__) (sv_ends_with)(SV(str__), SV(suffix__))

static ssize_t (sv_starts_with)(const struct string_view str, const struct string_view prefix)
{
    if (prefix.length >  str.length) { return 0; }
    return strncmp(str.data, prefix.data, prefix.length) == 0;
}

#define sv_starts_with(str__, prefix__) (sv_starts_with)(SV(str__), SV(prefix__))

bool (sv_equals)(const struct string_view a, const struct string_view b) {
    return a.length == b.length && strncmp(a.data, b.data, a.length) == 0;
}

#define sv_equals(a__, b__) (sv_equals)(SV(a__), SV(b__))

/// Matches the pointer and its const versions in a generic selector
/// @param type__: The type of the pointer
/// @param ptr__: The pointer to cast
#define MATCH_PTR_CAST_RETURN(type__, ptr__) \
    type__*: *((type__*)(ptr__)), \
    type__ *const: *((type__ *const)(ptr__)), \
    const type__*: *((const type__*)(ptr__)), \
    const type__ *const: *((const type__ *const)(ptr__))

/// Matches the pointer and its const versions in a generic selector
/// @param type__: The type of the pointer
/// @param do__: The code to execute if the type matches
#define MATCH_PTR(type__, do__) \
    type__*: (do__), \
    type__ *const: (do__), \
    const type__*: (do__), \
    const type__ *const: (do__)

/// Selects the correct string_view constructor based on the type of the input
/// @param str__: struct string_view | struct z_string | char* | char[] = The string to convert
/// @return A string_view
#define SV(str__) \
(REF(\
    _Generic(REFDECAY((str__)), \
        MATCH_PTR_CAST_RETURN(struct string_view, REFDECAY((str__))),\
        MATCH_PTR(struct z_string, sv_z(*(struct z_string *) REFDECAY((str__)))),\
        MATCH_PTR(char*, sv_cstr(*(char**) REFDECAY((str__))))\
    )\
)[0])

#define path_combine(...)\
({\
    const struct string_view __sv_args[] = { MAP_ARGS(SV, __VA_ARGS__) };\
    const size_t __len = sizeof __sv_args / sizeof __sv_args[0];\
    static char __path_buffer[PATH_MAX] = {};\
    memset(__path_buffer, 0, sizeof __path_buffer);\
    struct string_view __acc = (struct string_view){\
        .data = __path_buffer,\
        .length = 0,\
    };\
    for (size_t i = 0; i < __len; i++) {\
        sv_path_combine2_impl(&__acc, __acc, __sv_args[i]);\
    }\
    if (__acc.data[__acc.length - 1] == 0) {\
        __acc.length -= 1;\
    }\
    else {\
        __acc.data[__acc.length] = 0;\
    }\
    (struct z_string){ .data = __acc.data, .length = __acc.length };\
})

static struct string_view sv_concat_impl(const struct string_view dst, const int nargs, ...) {
    va_list args;
    va_start(args, nargs);
    struct string_view acc = dst;
    acc.length = 0;
    for (int i = 0; i < nargs; i++) {
        sv_concat2_impl(&acc, acc, va_arg(args, struct string_view));
    }
    va_end(args);
    return acc;
}

static struct z_string zconcat_impl(const struct string_view dst, const int nargs, ...) {
    va_list args;
    va_start(args, nargs);
    struct string_view acc = dst;
    acc.length = 0;
    for (int i = 0; i < nargs; i++) {
        sv_concat2_impl(&acc, acc, va_arg(args, struct string_view));
    }
    acc.data[acc.length] = 0;
    va_end(args);
    return *(struct z_string*) &acc;
}


#define sv_concat(...) sv_concat_impl(\
    sv_malloc(sum(MAP_ARGS(SVLEN, __VA_ARGS__))),\
    ARGS_COUNT(__VA_ARGS__),\
    MAP_ARGS(SV, __VA_ARGS__)\
)

#define zconcat(...) zconcat_impl(\
    sv_malloc(sum(MAP_ARGS(SVLEN, __VA_ARGS__))  + 1),\
    ARGS_COUNT(__VA_ARGS__),\
    MAP_ARGS(SV, __VA_ARGS__)\
)


static bool sv_comparer_equals(const void *a, const void *b) {
    return (sv_equals)(*(struct string_view *) a, *(struct string_view *) b);
}

#endif //STRING_VIEW_H
