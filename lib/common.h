#ifndef COMMON_H
#define COMMON_H

#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <PREPROCESSOR_CALCULUS.h>

#define var __auto_type

#define aligned_to(size__, alignment__) ({\
    var __size = (size__);\
    var __alignment = (alignment__);\
    (__size + __alignment-1) & ~(__alignment-1);\
})

#define aligned_sizeof_field(type__, member__) aligned_to((sizeof(((type__*)0)->member__)), __alignof(type__))

#define identity(x) x

#define MACRO_SELECT1(_1, NAME, ...) NAME
#define MACRO_SELECT2(_1, _2, NAME, ...) NAME
#define MACRO_SELECT3(_1, _2, _3, NAME, ...) NAME
#define MACRO_SELECT4(_1, _2, _3, _4, NAME, ...) NAME

#define clamp(value__, min__, max__) min2(max2((value__), (min__)), (max__))

#define container_of(ptr__, type__, member__) ((type__*) ((intptr_t)(ptr__) - offsetof(type__, member__)))
#define offsetof(type__, member__) ((size_t) &((type__*)0)->member__)

#define void_expression() ({;})

#define typeof_member(type__, member__) typeof(((type__*)0)->member__)

#define CONCAT_(x__, y__) x__ ## y__
#define CONCAT(x__, y__) CONCAT_(x__, y__)
#define LINE_VAR(name__) CONCAT(name__, __LINE__)

#define scope(begin__, ...) \
    for (bool LINE_VAR(_once_guard) = true; LINE_VAR(_once_guard); LINE_VAR(_once_guard) = false) \
    for (begin__; LINE_VAR(_once_guard); LINE_VAR(_once_guard) = false, ## __VA_ARGS__) \
    for (;LINE_VAR(_once_guard); LINE_VAR(_once_guard) = false)

#define finalizer(expression__) scoped_expression(, expression__)

#define STACK_BUFFER(size__, ...) ((uint8_t[(size__)]){ __VA_ARGS__ })

#define unreachable() __builtin_unreachable()

#define SVLEN(sv__) (SV((sv__)).length)

#define IS_INDEXABLE(arg) (sizeof(arg[0]))
#define IS_ARRAY_LIKE(arg) (((void *) &arg) == ((void *) arg))
#define IS_ARRAY(arg) (IS_INDEXABLE(arg) && IS_ARRAY_LIKE(arg))

#define ARRAY_LENGTH(array__) ({\
    assert(IS_ARRAY(array__));\
    (sizeof(array__)/sizeof(array__[0]));\
})


enum log_level {
    LOG_VERBOSE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
};

const enum log_level MIN_LOG_LEVEL = LOG_VERBOSE;

FILE *log_file = NULL;

#define TIMESTAMP_FORMAT "%Y-%m-%d %H:%M:%S %Z"

_Thread_local static char __timestamp_buffer[128];

static char *timestamp() {
    time_t rt;
    time(&rt);
    const struct tm *tp = localtime(&rt);
    strftime(__timestamp_buffer, sizeof __timestamp_buffer, TIMESTAMP_FORMAT, tp);
    return __timestamp_buffer;
}

static char *log_level_cstr(const enum log_level level) {
    switch (level) {
    case LOG_VERBOSE: return "VERBOSE";
    case LOG_DEBUG: return "DEBUG";
    case LOG_INFO: return "INFO";
    case LOG_WARNING: return "WARNING";
    case LOG_ERROR: return "ERROR";
    default: return "INVALID";
    }
}

#define diagf(LOG_LEVEL__, fmt__, ...)\
({\
    if (log_file == NULL) { log_file = stdout; }\
    var log_level = (LOG_LEVEL__);\
    if (log_level >= MIN_LOG_LEVEL) {\
        fprintf(log_file, "[%s] [%s:%d] [%s] " fmt__, timestamp(), __FILE__, __LINE__, log_level_cstr(log_level), ## __VA_ARGS__);\
    }\
})
#define diag(LOG_LEVEL__, msg__) diagf(LOG_LEVEL__, "%s\n", msg__)
#define ediagf(fmt__, ...) diagf(LOG_ERROR, "%s: "fmt__, strerror(errno), ## __VA_ARGS__)
#define ediag(msg__) diagf(LOG_ERROR, "%s: %s\n", strerror(errno), msg__)

static size_t next_power_of_two(const size_t value) {
    size_t result = 1;
    while (result < value) {
        result <<= 1;
    }
    return result;
}

static bool is_prime(const int n) {
    // Corner cases
    if (n <= 1) return false;
    if (n <= 3) return true;

    if (n % 2 == 0 || n % 3 == 0) return false;

    for (int i = 5; i * i <= n; i = i + 6)
        if (n % i == 0 || n % (i + 2) == 0)
            return false;

    return true;
}

static int next_prime(const int n) {
    if (n <= 1)
        return 2;

    int prime = n;
    bool found = false;

    while (!found) {
        prime++;

        if (is_prime(prime))
            found = true;
    }

    return prime;
}

static int32_t string_hash(const void *str_ref) {
    const char *str = *(char **) str_ref;
    uint32_t hash = 0x12345678;
    for (; *str; ++str) {
        hash ^= *str;
        hash *= 0x5bd1e995;
        hash ^= hash >> 15;
    }
    return hash;
}

static bool string_comparer_equals(const void *a, const void *b) {
    const char *str_a = *(char **) a;
    const char *str_b = *(char **) b;
    return strcmp(str_a, str_b) == 0;
}

#define size_and_address(struct__) sizeof(struct__), (void*)&(struct__)

/// Checks if the given value is zero initialized aka all of its bytes are zero
/// @param x: The value to check
/// @return True if the value is zero initialized, false otherwise
#define zero_initialized(x) ({\
    typeof(x) _x = (x);\
    memcmp(&_x, &((typeof(_x)){}), sizeof _x) == 0;\
})

#define zero(type__) (typeof(type__)){0}

#define lambda(return_type, function_body) \
({ \
    return_type _fn_ function_body \
    _fn_; \
})

/// Creates a stack array of size 1 with the given value in it
/// @param value__: The value to store in the array
/// @return A stack array of size 1 with the given value in it
///
/// example:
///
/// int *ptr = &1; // Absolutly not ❌
///
/// int *ptr = REF(1); // A okay ✅
#define REF(value__) ((typeof(value__)[1]){(value__)})

/// Creates a stack array of size 1 with the given value in it, If the value is an array decays it into a pointer
/// @param array__: The value to store in the array
/// @return A stack array of size 1 with the given value in it
///
/// example:
///
/// typeof(REF("ABC")) == char(*)[4]
///
/// typeof(REFDECAY("ABC")) == char*
#define REFDECAY(array__) ((typeof(({ array__; }))[1]){(array__)})

#define min2(a, b) ({\
    var min_a = (a);\
    var min_b = (b);\
    min_a < min_b ? min_a : min_b;\
})

#define max2(a_, b_) ({\
    var __max_a = (a_);\
    var __max_b = (b_);\
    __max_a > __max_b ? __max_a : __max_b;\
})

#define TUPLE_LAZY(...)\
    IF(HAS_ARGS(__VA_ARGS__))\
    (\
        DEFER2(TUPLE_LAZY_REC)() (TAIL(__VA_ARGS__))\
        HEAD(__VA_ARGS__) CONCAT(item, ARGS_COUNT(__VA_ARGS__));\
    )
#define TUPLE_LAZY_REC() TUPLE_LAZY

#define tuple(...) struct { EVAL(TUPLE_LAZY(ARGS_REVERSE(__VA_ARGS__))) }
#define packed_tuple(...) struct { EVAL(TUPLE_LAZY(ARGS_REVERSE(__VA_ARGS__))) } __attribute__((packed))

#define tuple_size(...) sizeof(struct { EVAL(TUPLE_LAZY(__VA_ARGS__)) })
#endif //COMMON_H
