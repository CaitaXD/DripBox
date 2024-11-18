#ifndef MONITOR_H
#define MONITOR_H

#include <pthread.h>

#ifndef MONITOR_API
#   define MONITOR_API static inline
#endif

struct monitor_t {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

#define MONITOR_INITIALIZER (struct monitor_t){ \
    .mutex = PTHREAD_MUTEX_INITIALIZER,\
    .cond = PTHREAD_COND_INITIALIZER,\
}

MONITOR_API void monitor_enter(struct monitor_t *monitor);
MONITOR_API void monitor_exit(struct monitor_t *monitor);
MONITOR_API void monitor_notify_one(struct monitor_t *monitor);
MONITOR_API void monitor_notify_all(struct monitor_t *monitor);
MONITOR_API void monitor_wait_one(struct monitor_t *monitor);

#define using_monitor(monitor__) \
    scope(monitor_enter(monitor__), monitor_exit(monitor__))

#define monitor_wait(monitor__, condition__) \
      while (!(condition__)) monitor_wait_one(monitor__)

#define builtin_pause() __builtin_ia32_pause()

#define busy_wait(expression__) \
    while (!(expression__)) finalizer_expression(builtin_pause();)

void monitor_enter(struct monitor_t *monitor) {
    pthread_mutex_lock(&monitor->mutex);
}

void monitor_exit(struct monitor_t *monitor) {
    pthread_mutex_unlock(&monitor->mutex);
}

void monitor_notify_one(struct monitor_t *monitor) {
    pthread_cond_signal(&monitor->cond);
}

void monitor_notify_all(struct monitor_t *monitor) {
    pthread_cond_broadcast(&monitor->cond);
}

void monitor_wait_one(struct monitor_t *monitor) {
    pthread_cond_wait(&monitor->cond, &monitor->mutex);
}

#endif //MONITOR_H
