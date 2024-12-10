// ReSharper disable CppDFAUnusedValue
// Reason: Static analysis cannot correcty analize a coroutine's control flow

#ifndef COROUTINE_H
#define COROUTINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <Allocator.h>
#include <dynamic_array.h>
#include <array.h>

struct coroutine {
    enum {
        CO_CREATED,
        CO_RUNNING,
        CO_COMPLETED_SUCESSFULY,
        CO_FAILED,
    } co_state;
    void *label;
    void *return_value;
    void* (*function)(struct coroutine *, ...);
    struct allocator_arena *stack;
};

enum { CO_DEFAULT_STACK_SIZE = 512 };

#define co_stack(co_stack_size__) (struct coroutine) { \
    .stack = REF(allocator_stack_arena((co_stack_size__))),\
}

#define co_malloc(co_stack_size__) (struct coroutine) { \
    .stack = REF(allocator_malloc_arena((co_stack_size__))),\
}

#define co_temp(co_stack_size__) REF(({\
    var temp_arena = allocator_temp_arena();\
    var frame = *temp_arena;\
    temp_arena->current_address += co_stack_size__;\
    struct coroutine co = {\
        .stack = frame,\
    };\
}))

#define co_detach_on_init1(co__, type__)\
    (((co__)->co_state == CO_CREATED) \
        ? (type__*)allocator_arena_detach_bytes((co__)->stack, -sizeof(type__))\
        : (type__*)NULL)

#define co_detach_on_init2(co__, type__, CLITERAL)\
    (((co__)->co_state == CO_CREATED) \
        ? (type__*)memcpy(allocator_arena_detach_bytes((co__)->stack, -sizeof(type__)), &CLITERAL, sizeof (type__)) \
        : (type__*)NULL)

#define co_detach1(co__, type__) \
    ((type__*)allocator_arena_detach_bytes((co__)->stack, -sizeof(type__)))

#define co_detach2(co__, type__, CLITERAL) \
    ((type__*)memcpy(allocator_arena_detach_bytes((co__)->stack, -sizeof(type__)), &CLITERAL, sizeof (type__)))

#define co_detach(...) \
    MACRO_SELECT3(__VA_ARGS__, co_detach2, co_detach1, co_detach1, co_detach1)(__VA_ARGS__)

/// Allocates memory for a variable in the coroutine scope when the coroutine is created
///
/// Reserves from the end of the coroutine stack
///
/// This does not save the variable in the coroutine stack
///
/// You still need to save the variable in the coroutine stack
///
/// @param co__: struct coroutine* -> The coroutine handle
/// @param type__: any -> The type of the variable
/// @param CLITERAL: any -> The value to store in the variable
/// @return The value of the variable
#define co_detach_on_init(...) MACRO_SELECT3(__VA_ARGS__, co_detach_on_init2, co_detach_on_init1, co_detach_on_init1, co_detach_on_init1)(__VA_ARGS__)

/// Initializes a variable only when coroutine is created
///
/// This does not save the variable in the coroutine stack
///
/// You still need to save the variable in the coroutine stack
///
/// @param co__: struct coroutine* -> The coroutine handle
/// @param value__: any -> The value to store in the variable
/// @return The value of the variable
#define co_assign_on_init(co__, value__) \
    (((co__)->co_state == CO_CREATED) ? (value__) : zero(typeof(value__)))

#define CO_SAVE(co__, ...)\
IF(HAS_ARGS(__VA_ARGS__))\
(\
    HEAD(__VA_ARGS__) = ({\
        typedef typeof(HEAD(__VA_ARGS__)) T;\
        var _co = (co__);\
        if (_co->stack == NULL) {\
            var _arena = allocator_malloc_arena(CO_DEFAULT_STACK_SIZE);\
            _co->stack = memcpy(allocator_arena_detach_bytes(&_arena, sizeof _arena), &_arena, sizeof _arena);\
        }\
        T* _var = allocator_arena_alloc_aligned(_co->stack, sizeof(T), __alignof(T));\
        *_var = HEAD(__VA_ARGS__);\
    });\
    DEFER2(CO_SAVE_REC)() (co__, TAIL(__VA_ARGS__))\
)
#define CO_SAVE_REC() CO_SAVE

#define CO_LOAD(co__, ...)\
IF(HAS_ARGS(__VA_ARGS__))\
(\
    HEAD(__VA_ARGS__) = ({\
        typedef typeof(HEAD(__VA_ARGS__)) T;\
        var _co = (co__);\
        if (_co->stack == NULL) {\
            var _arena = allocator_malloc_arena(CO_DEFAULT_STACK_SIZE);\
            _co->stack = memcpy(allocator_arena_detach_bytes(&_arena, sizeof _arena), &_arena, sizeof _arena);\
        }\
        T* _var = allocator_arena_alloc_aligned(_co->stack, sizeof(T), __alignof(T));\
        *_var; \
    });\
    DEFER2(CO_LOAD_REC)() (co__, TAIL(__VA_ARGS__))\
)
#define CO_LOAD_REC() CO_LOAD

#define CO_BEGIN(co__, function__, ...)\
({\
    var _co = (co__);\
    _co->function = (void*)(function__);\
    if (_co->co_state == CO_CREATED) { \
        EVAL(CO_SAVE(_co, __VA_ARGS__));\
    }\
    else { \
        allocator_arena_clear(co__->stack);\
        EVAL(CO_LOAD(_co, __VA_ARGS__));\
    }\
    if (co_is_completed(_co)) { \
        co_yield_break(_co);\
    }\
    if (_co->co_state == CO_CREATED) { \
        _co->co_state = CO_RUNNING;\
        _co->label = NULL;\
        allocator_arena_clear(_co->stack);\
        goto _COROUTINE_YIELD_LABEL;\
    }\
    else if (_co->co_state == CO_RUNNING && _co->label != NULL) { \
        goto *(_co->label);\
    }\
})

#define CO_END(co__) ({\
    var _co = (co__);\
    if (_co->co_state != CO_FAILED) { \
        _co->co_state = CO_COMPLETED_SUCESSFULY;\
    }\
    allocator_arena_clear(_co->stack);\
})

/// Initiates a coroutine scope
/// @param co__: struct coroutine* -> The coroutine handle
/// @param function__: void*(*)(struct coroutine*) -> The function that will execute the coroutine
/// @param ...: any -> Local variables to be *lifted into the coroutine
/// @remark *lifting is when a local variable in the function stack is copied into the coroutine stack
#define COROUTINE(co__, function__, ...)\
    var LINE_VAR(_coroutine_handle) = (co__);\
    goto _COROUTINE_SCOPE_LABEL;\
    _COROUTINE_YIELD_LABEL:\
    EVAL(CO_SAVE(LINE_VAR(_coroutine_handle), __VA_ARGS__));\
    return LINE_VAR(_coroutine_handle)->return_value;\
    _COROUTINE_SCOPE_LABEL:\
    scope(CO_BEGIN(LINE_VAR(_coroutine_handle), (function__), ## __VA_ARGS__), CO_END(LINE_VAR(_coroutine_handle)))

#define co_yield_break(co__) {\
    var _co = (co__);\
    _co->label = NULL;\
    allocator_arena_clear(_co->stack);\
    _co->co_state = CO_COMPLETED_SUCESSFULY;\
    goto _COROUTINE_YIELD_LABEL;\
}

#define co_yield_return(co__, ret__) ({\
    var _co = (co__);\
    _co->label = &&LINE_VAR(state);\
    allocator_arena_clear(_co->stack);\
    memcpy(_co->return_value, &ret__, sizeof ret__);\
    goto _COROUTINE_YIELD_LABEL;\
    LINE_VAR(state):\
})

#define co_yield(co__) ({\
    var _co = (co__);\
    _co->label = &&LINE_VAR(state);\
    allocator_arena_clear(_co->stack);\
    goto _COROUTINE_YIELD_LABEL;\
    LINE_VAR(state):\
})

#define co_return(co__, ret__) ({\
    typedef typeof(ret__) T;\
    var _co = (co__);\
    _co->return_value = ({\
        if (_co->stack == NULL) {\
            var _arena = allocator_malloc_arena(CO_DEFAULT_STACK_SIZE);\
            _co->stack = memcpy(allocator_arena_detach_bytes(&_arena, sizeof _arena), &_arena, sizeof _arena);\
        }\
        T *_var = allocator_arena_alloc_aligned(_co->stack, sizeof(T), __alignof(T));\
        _var;\
    });\
    memcpy(_co->return_value, &ret__, sizeof (ret__));\
    _co->label = NULL;\
    allocator_arena_clear(_co->stack);\
    _co->co_state = CO_COMPLETED_SUCESSFULY;\
    goto _COROUTINE_YIELD_LABEL;\
})

static void co_clear(struct coroutine *co) {
    co->co_state = CO_CREATED;
    co->label = NULL;
    co->function = NULL;
    if (co->stack != NULL) allocator_arena_clear(co->stack);
}

static bool co_is_completed(const struct coroutine *co) {
    return co->co_state == CO_COMPLETED_SUCESSFULY || co->co_state == CO_FAILED;
}

static bool co_has_next_stage(const struct coroutine *co) {
    return co->co_state == CO_COMPLETED_SUCESSFULY && co->label != NULL;
}

static void* co_resume(struct coroutine *co) {
    return co->function(co);
}

static void* co_wait(struct coroutine *co) {
    void *ret = NULL;
    do  {
        ret = co_resume(co);
    } while (!co_is_completed(co));
    return ret;
}

static void ** co_resume_all(array(struct coroutine) coroutines, array(void*) results) {
    assert(array_length(coroutines) >= (results ? array_length(results) : 0));
    for (int i = 0; i < array_length(coroutines); i += 1) {
        struct coroutine *coroutine = &coroutines[i];
        if (!co_is_completed(coroutine)) {
            void *result = co_resume(coroutine);
            if(results) {
                results[i] = result;
            }
        }
    }
    return results;
}

static void** co_when_all(struct coroutine* co, array(struct coroutine) coroutines, array(void*) results) {
    COROUTINE(co, co_when_all, coroutines, results) {
        while (true) {
            co_resume_all(coroutines, results);
            co_yield(co);
            int completed = 0;
            for (int j = 0; j < array_length(coroutines); j++) {
                if (co_is_completed(&coroutines[j])) {
                    completed += 1;
                }
            }
            if (completed == array_length(coroutines)) {
                break;
            }
        }
    }
    return results;
}

static void **co_wait_all(array(struct coroutine) coroutines, array(void*) results) {
    struct coroutine co = {};
    co_when_all(&co, coroutines, results);
    co_wait(&co);
    return results;
}

static void* co_sleep(struct coroutine *co, int seconds) {
    const time_t now = time(NULL);
    time_t start = now;
    time_t elapsed = 0;
    COROUTINE(co, co_sleep, seconds, start, elapsed) {
        while (elapsed < seconds) {
            elapsed = now - start;
            co_yield(co);
        }
    }
    return NULL;
}

static void* co_continue_with(struct coroutine *co, struct coroutine *other, void* (*callback)(void*ctx), void *ctx) {
    COROUTINE(co, co_continue_with, other, callback, ctx) {
        while (other->co_state != CO_COMPLETED_SUCESSFULY) {
            co_resume(other);
            co_yield(co);
        }
    }
    return callback(ctx);
}

static void* co_queue_dispatch(struct coroutine *co, struct queue coroutines) {
    COROUTINE(co, co_queue_dispatch, coroutines) {
        struct coroutine *current = NULL;
        while (fifo_try_pop(coroutines, &current)) {
            assert(current->function && "Function pointer is null");
            co_resume(current);
            if (current->co_state != CO_COMPLETED_SUCESSFULY) {
                fifo_push(coroutines, current);
            }
            co_yield(co);
        }
    }
    return NULL;
}

static struct coroutine* co_detach_nested_coroutine_on_init(const struct coroutine *co, const ssize_t size) {
    if (co->co_state == CO_CREATED) {
        struct allocator_arena *nested_stack = allocator_arena_detach_bytes(co->stack, -sizeof(struct allocator_arena));
        *nested_stack = allocator_arena_detach_frame(co->stack, -(size - sizeof(struct allocator_arena)));
        return co_detach_on_init(co, struct coroutine, (struct coroutine) {
            .stack = nested_stack
        });
    }
    return NULL;
}

static struct coroutine* co_detach_nested_coroutine(const struct coroutine *co, const ssize_t size) {
    struct allocator_arena *nested_stack = allocator_arena_detach_bytes(co->stack, -sizeof(struct allocator_arena));
    *nested_stack = allocator_arena_detach_frame(co->stack, -(size - sizeof(struct allocator_arena)));
    return co_detach_on_init(co, struct coroutine, (struct coroutine) {
        .stack = nested_stack
    });
}

#endif //COROUTINE_H
