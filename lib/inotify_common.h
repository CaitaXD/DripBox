#ifndef INOTIFY_COMMON_H
#define INOTIFY_COMMON_H

#include <stdio.h>
#include <sys/inotify.h>

#ifndef INOTIFY_COMMON_API
#   define INOTIFY_COMMON_API static inline
#endif

struct inotify_watcher_t {
    int inotify_fd;
    int watcher_fd;
    struct timeval timeout_timer;
    fd_set read_fds;
    int inotify_event_return;
};

INOTIFY_COMMON_API struct inotify_watcher_t inotify_watcher_new(int inotify_initial_fd);

INOTIFY_COMMON_API struct inotify_watcher_t init_inotify(int inotify_initial_fd, char dir[]);

INOTIFY_COMMON_API void read_event(struct inotify_watcher_t watcher);

#define EVENT_SIZE (sizeof (struct inotify_event))

#define EVENT_BUFFER_LEN (1024 * (EVENT_SIZE + 16))

struct inotify_watcher_t inotify_watcher_new(int inotify_initial_fd) {
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

struct inotify_watcher_t init_inotify(int inotify_initial_fd, char dir[]){

    struct inotify_watcher_t watcher_inst = inotify_watcher_new(inotify_initial_fd);

    if(watcher_inst.inotify_fd < 0){
        watcher_inst.inotify_fd = inotify_init();
        if(watcher_inst.inotify_fd < 0) {
            log(LOG_ERROR, "Inotify init: %s\n", strerror(errno));
            return watcher_inst;
        }
    }

    int test = inotify_add_watch(watcher_inst.inotify_fd, dir, IN_MOVE | IN_MODIFY | IN_ATTRIB | IN_DELETE | IN_DELETE_SELF); // 
    if(test < 0){
        printf("erro");
        log(LOG_ERROR, "Inotify watch add: %s\n", strerror(errno));
        return watcher_inst;
    }
    watcher_inst.watcher_fd = test;
        
    FD_ZERO (&watcher_inst.read_fds);

    FD_SET (watcher_inst.inotify_fd, &watcher_inst.read_fds);

    return watcher_inst;
}

void read_event(struct inotify_watcher_t watcher){

    char buf[EVENT_BUFFER_LEN];
    int len, i = 0;

    len = read (watcher.inotify_fd, buf, EVENT_BUFFER_LEN);
    if (len < 0) {
        log(LOG_ERROR, "Read Inotify watcher event: %s\n", strerror(errno));
    } else if (!len)
        printf("event buffet too small apparently");

    while (i < len) {
        struct inotify_event *event;

        event = (struct inotify_event *) &buf[i];

        printf ("wd=%d mask=%u cookie=%u len=%u\n",
            event->wd, event->mask,
            event->cookie, event->len);

        if (event->len)
            printf ("name=%s\n", event->name);

        if(event->mask >= IN_ISDIR){
            printf("dir action\n");
            event->mask %=IN_ISDIR;
        }

        switch(event->mask){
            case IN_MODIFY: printf("IN_MODIFY\n");
            break;
            case IN_ATTRIB: printf("IN_ATTRIB\n");
            break;
            case IN_MOVED_TO: printf("IN_MOVED_TO\n");
            break;
            case IN_MOVED_FROM: printf("IN_MOVED_FROM\n");
            break;
            case IN_DELETE: printf("IN_DELETE\n");
            break;
            case IN_DELETE_SELF: printf("IN_DELETE_SELF\n");
            break;
            default: printf("OTHER\n");
        }
        i += EVENT_SIZE + event->len;
    }
}

int select_inotify_event(struct inotify_watcher_t watcher) {
    watcher.inotify_event_return = select(watcher.inotify_fd + 1, &watcher.read_fds, NULL, NULL, &watcher.timeout_timer);
    if (watcher.inotify_event_return < 0) {
        log(LOG_ERROR, "Inotify event select: %s\n", strerror(errno));
        return -1;
    }
    else if(!watcher.inotify_event_return) {
        printf("idk what happens test lol");
    }
    else if (FD_ISSET (watcher.inotify_fd, &watcher.read_fds)) {
        read_event(watcher);
    }

    return 0;
}

void* inotify_watcher_loop(){
    struct inotify_watcher_t watcher = init_inotify(-1, "./sync_dir");

    while(1){
        read_event(watcher);
    }
    
    int ret_value = inotify_rm_watch (watcher.inotify_fd, watcher.watcher_fd);
    if (ret_value)
        log(LOG_ERROR, "Inotify watch remove: %s\n", strerror(errno));
        return -1;
    
    return 0;
}





#endif //INOTIFY_COMMON_H