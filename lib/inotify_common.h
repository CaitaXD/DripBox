#ifndef INOTIFY_COMMON_H
#define INOTIFY_COMMON_H

#include <stdio.h>
#include <sys/inotify.h>

#ifndef INOTIFY_COMMON_API
#   define INOTIFY_COMMON_API static inline
#endif


#define EVENT_SIZE (sizeof (struct inotify_event))

#define EVENT_BUFFER_LEN (1024 * (EVENT_SIZE + 16))

struct inotify_watcher_t {
    int inotify_fd;
    int watcher_fd;
    struct timeval timeout_timer;
    fd_set read_fds;
    int inotify_event_return;
};

struct inotify_event_t {
    char event_buffer[EVENT_BUFFER_LEN];
    int buffer_len;
    int error;
};

INOTIFY_COMMON_API struct inotify_watcher_t inotify_watcher_new(int inotify_initial_fd);

INOTIFY_COMMON_API struct inotify_watcher_t init_inotify(int inotify_initial_fd, char dir[]);

INOTIFY_COMMON_API struct inotify_event_t read_event(struct inotify_watcher_t watcher);

struct inotify_watcher_t inotify_watcher_new(const int inotify_initial_fd) {
    const struct inotify_watcher_t watcher_inst = {
        .inotify_fd = inotify_initial_fd,
        .watcher_fd = -1,
        .timeout_timer = (struct timeval){
            .tv_sec = 5, // 5 seconds until timeout
            .tv_usec = 0
        },
        .read_fds = -1,
        .inotify_event_return = -1
    };
    return watcher_inst;
}

struct inotify_watcher_t init_inotify(const int inotify_initial_fd, char dir[]) {
    struct inotify_watcher_t watcher_inst = inotify_watcher_new(inotify_initial_fd);

    if (watcher_inst.inotify_fd < 0) {
        watcher_inst.inotify_fd = inotify_init();
        if (watcher_inst.inotify_fd < 0) {
            diagf(LOG_ERROR, "Inotify init: %s\n", strerror(errno));
            return watcher_inst;
        }
    }

    const int fd = inotify_add_watch(watcher_inst.inotify_fd, dir,
                                       IN_MOVE | IN_MODIFY | IN_ATTRIB | IN_DELETE | IN_DELETE_SELF); //
    if (fd < 0) {
        printf("erro");
        diagf(LOG_ERROR, "Inotify watch add: %s\n", strerror(errno));
        return watcher_inst;
    }
    watcher_inst.watcher_fd = fd;

    FD_ZERO(&watcher_inst.read_fds);

    FD_SET(watcher_inst.inotify_fd, &watcher_inst.read_fds);

    return watcher_inst;
}

struct inotify_event_t read_event(const struct inotify_watcher_t watcher) {
    struct inotify_event_t inotify_event = {0};
 
    inotify_event.buffer_len = read(watcher.inotify_fd, inotify_event.event_buffer, EVENT_BUFFER_LEN);
    if (inotify_event.buffer_len < 0) {
        inotify_event.error = errno;
        if (inotify_event.error == EAGAIN) { return inotify_event; }

        diagf(LOG_ERROR, "Read Inotify watcher event: %s\n", strerror(errno));
    } else if (!inotify_event.buffer_len) {
        printf("event buffet too small apparently");
    }
    return inotify_event;
}

#endif //INOTIFY_COMMON_H
