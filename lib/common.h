#ifndef COMMON_H
#define COMMON_H

#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define var __auto_type

#define aligned_to(size__, alignment__) ({\
    var __size = (size__);\
    var __alignment = (alignment__);\
    (__size + __alignment-1) & ~(__alignment-1);\
})

#define aligned_sizeof_field(type__, member__) aligned_to((sizeof(((type__*)0)->member__)), __alignof(type__))

#define identity(x) x

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

#define max3(a_, b_, c_) max2(a_, max2(b_, c_))

#define MACRO_SELECT1(_1, NAME, ...) NAME
#define MACRO_SELECT2(_1, _2, NAME, ...) NAME
#define MACRO_SELECT3(_1, _2, _3, NAME, ...) NAME

#define max(...) MACRO_SELECT3(__VA_ARGS__, max3, max2, identity)(__VA_ARGS__)
#define min(...) MACRO_SELECT2(__VA_ARGS__, min2, identity)(__VA_ARGS__)

#define clamp(value__, min__, max__) min2(max2((value__), (min__)), (max__))

#define container_of(ptr__, type__, member__) ((type__*) ((intptr_t)(ptr__) - offsetof(type__, member__)))
#define offsetof(type__, member__) ((size_t) &((type__*)0)->member__)

#define void_expression() ({;})

#define typeof_member(type__, member__) typeof(((type__*)0)->member__)

#define CONCAT_(x__, y__) x__ ## y__
#define CONCAT(x__, y__) CONCAT_(x__, y__)
#define LINE_VAR(name__) CONCAT(name__, __LINE__)

#define scope(begin__, ...) \
    for (bool LINE_VAR(__once_guard) = true; LINE_VAR(__once_guard); LINE_VAR(__once_guard) = false) \
    for (begin__; LINE_VAR(__once_guard); LINE_VAR(__once_guard) = false, ({__VA_ARGS__;}))

#define finalizer(expression__) scoped_expression(, expression__)

#define ARRAY_LITERAL(size__, ...) (uint8_t[(size__)]){ __VA_ARGS__ }

#define unreachable() __builtin_unreachable()

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

#define log(LOG_LEVEL__, fmt__, ...)\
  ({\
    if (log_file == NULL) { log_file = stdout; }\
    var log_level = (LOG_LEVEL__);\
    if (log_level >= MIN_LOG_LEVEL) {\
        fprintf(log_file, "[%s] [%s:%d] " fmt__ "\n", timestamp(), __FILE__, __LINE__, ## __VA_ARGS__);\
    }\
  })

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
    int32_t hash = 0x12345678;
    for (; *str; ++str) {
        hash ^= *str;
        hash *= 0x5bd1e995;
        hash ^= hash >> 15;
    }
    return hash;
}

static bool string_equals(const void *a, const void *b) {
    const char *str_a = *(char **) a;
    const char *str_b = *(char **) b;
    return strcmp(str_a, str_b) == 0;
}

#define size_and_address(struct__) sizeof(struct__), (uint8_t*)&(struct__)

#define MAP0(fn, dummy)
#define MAP1(fn, a) fn(a)
#define MAP2(fn, a, b) fn(a), fn(b)
#define MAP3(fn, a, b, c) fn(a), fn(b), fn(c)
#define MAP4(fn, a, b, c, d) fn(a), fn(b), fn(c), fn(d)
#define MAP5(fn, a, b, c, d, e) fn(a), fn(b), fn(c), fn(d), fn(e)
#define MAP6(fn, a, b, c, d, e, f) fn(a), fn(b), fn(c), fn(d), fn(e) ,fn(f)

#define ARGS_COUNT_(dummy, x6, x5, x4, x3, x2, x1, x0, ...) x0
#define ARGS_COUNT(...) ARGS_COUNT_(dummy, ##__VA_ARGS__, 6, 5, 4, 3, 2, 1, 0)

#define MAP__(fn, n, ...) MAP##n(fn, __VA_ARGS__)
#define MAP_(fn, n, ...) MAP__(fn, n, __VA_ARGS__)
#define MAP(fn, ...) MAP_(fn, ARGS_COUNT(__VA_ARGS__), __VA_ARGS__)

#endif //COMMON_H
