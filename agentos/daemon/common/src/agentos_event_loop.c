#include "agentos_event_loop.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
#include "platform.h"

agentos_event_loop_t* agentos_event_loop_create(int max_events) {
    (void)max_events;
    LOG_ERROR("agentos_event_loop: epoll not available on Windows");
    errno = ENOSYS;
    return NULL;
}

void agentos_event_loop_destroy(agentos_event_loop_t* loop) { (void)loop; }
int agentos_event_loop_add_fd(agentos_event_loop_t* loop, int fd, uint32_t events,
                               agentos_event_callback_t cb, void* user_data) {
    (void)loop; (void)fd; (void)events; (void)cb; (void)user_data;
    return -ENOSYS;
}
int agentos_event_loop_add_fd_lt(agentos_event_loop_t* loop, int fd, uint32_t events,
                                  agentos_event_callback_t cb, void* user_data) {
    (void)loop; (void)fd; (void)events; (void)cb; (void)user_data;
    return -ENOSYS;
}
int agentos_event_loop_mod_fd(agentos_event_loop_t* loop, int fd, uint32_t events) {
    (void)loop; (void)fd; (void)events; return -ENOSYS;
}
void agentos_event_loop_remove_fd(agentos_event_loop_t* loop, int fd) { (void)loop; (void)fd; }
uint64_t agentos_event_loop_add_timer(agentos_event_loop_t* loop, uint64_t interval_ms,
                                       agentos_timer_callback_t cb, void* user_data) {
    (void)loop; (void)interval_ms; (void)cb; (void)user_data; errno = ENOSYS; return 0;
}
int agentos_event_loop_cancel_timer(agentos_event_loop_t* loop, uint64_t timer_id) {
    (void)loop; (void)timer_id; return -ENOSYS;
}
int agentos_event_loop_run(agentos_event_loop_t* loop) { (void)loop; return -ENOSYS; }
void agentos_event_loop_stop(agentos_event_loop_t* loop) { (void)loop; }
int agentos_event_loop_wakeup(agentos_event_loop_t* loop) { (void)loop; return -ENOSYS; }
int agentos_event_loop_get_fd_count(agentos_event_loop_t* loop) { (void)loop; return -ENOSYS; }

#else

#include <unistd.h>
#include <sys/epoll.h>
#ifdef __linux__
#include <sys/eventfd.h>
#endif

#include "svc_logger.h"

typedef struct {
    uint64_t id;
    uint64_t interval_ms;
    uint64_t next_fire_ms;
    agentos_timer_callback_t cb;
    void* user_data;
    bool active;
} timer_entry_t;

typedef struct {
    int fd;
    uint32_t events;
    agentos_event_callback_t cb;
    void* user_data;
    bool level_triggered;
} fd_entry_t;

struct agentos_event_loop {
    int epoll_fd;
    int wakeup_fd;
    int max_events;
    struct epoll_event* epoll_events;
    fd_entry_t* fd_entries;
    timer_entry_t timers[AGENTOS_EVENT_LOOP_MAX_TIMERS];
    uint64_t next_timer_id;
    uint64_t current_time_ms;
    volatile bool running;
    volatile bool stop_requested;
};

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static int add_fd_internal(agentos_event_loop_t* loop, int fd, uint32_t events,
                            agentos_event_callback_t cb, void* user_data, bool level_triggered) {
    if (!loop || fd < 0 || !cb) return -1;
    if (fd >= loop->max_events) {
        LOG_DEBUG("fd=%d exceeds max_events=%d, cannot track callback", fd, loop->max_events);
        return -1;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;

    if (events & AGENTOS_EVENT_TYPE_READ)  ev.events |= EPOLLIN;
    if (events & AGENTOS_EVENT_TYPE_WRITE) ev.events |= EPOLLOUT;
    if (!level_triggered) ev.events |= EPOLLET;

    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        if (errno == EEXIST) {
            if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                LOG_DEBUG("epoll_ctl MOD failed for fd=%d: %s", fd, strerror(errno));
                return -1;
            }
        } else {
            LOG_DEBUG("epoll_ctl ADD failed for fd=%d: %s", fd, strerror(errno));
            return -1;
        }
    }

    loop->fd_entries[fd].fd = fd;
    loop->fd_entries[fd].events = events;
    loop->fd_entries[fd].cb = cb;
    loop->fd_entries[fd].user_data = user_data;
    loop->fd_entries[fd].level_triggered = level_triggered;

    return 0;
}

agentos_event_loop_t* agentos_event_loop_create(int max_events) {
    agentos_event_loop_t* loop = (agentos_event_loop_t*)calloc(1, sizeof(agentos_event_loop_t));
    if (!loop) return NULL;

    if (max_events <= 0) max_events = AGENTOS_EVENT_LOOP_MAX_EVENTS;

    loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (loop->epoll_fd < 0) {
        free(loop);
        return NULL;
    }

    loop->max_events = max_events;
    loop->epoll_events = (struct epoll_event*)calloc((size_t)max_events, sizeof(struct epoll_event));
    loop->fd_entries = (fd_entry_t*)calloc((size_t)max_events, sizeof(fd_entry_t));

    if (!loop->epoll_events || !loop->fd_entries) {
        close(loop->epoll_fd);
        free(loop->epoll_events);
        free(loop->fd_entries);
        free(loop);
        return NULL;
    }

    loop->wakeup_fd = -1;
#ifdef __linux__
    loop->wakeup_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (loop->wakeup_fd >= 0) {
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.data.fd = loop->wakeup_fd;
        ev.events = EPOLLIN;
        if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, loop->wakeup_fd, &ev) < 0) {
            close(loop->wakeup_fd);
            loop->wakeup_fd = -1;
        }
    }
#endif

    loop->next_timer_id = 1;
    for (int i = 0; i < AGENTOS_EVENT_LOOP_MAX_TIMERS; i++) {
        loop->timers[i].active = false;
    }

    LOG_DEBUG("Event loop created (epoll_fd=%d, wakeup_fd=%d, max_events=%d)",
              loop->epoll_fd, loop->wakeup_fd, max_events);
    return loop;
}

void agentos_event_loop_destroy(agentos_event_loop_t* loop) {
    if (!loop) return;
    if (loop->wakeup_fd >= 0) close(loop->wakeup_fd);
    close(loop->epoll_fd);
    free(loop->epoll_events);
    free(loop->fd_entries);
    free(loop);
}

int agentos_event_loop_add_fd(agentos_event_loop_t* loop, int fd, uint32_t events,
                               agentos_event_callback_t cb, void* user_data) {
    return add_fd_internal(loop, fd, events, cb, user_data, false);
}

int agentos_event_loop_add_fd_lt(agentos_event_loop_t* loop, int fd, uint32_t events,
                                  agentos_event_callback_t cb, void* user_data) {
    return add_fd_internal(loop, fd, events, cb, user_data, true);
}

int agentos_event_loop_mod_fd(agentos_event_loop_t* loop, int fd, uint32_t events) {
    if (!loop || fd < 0 || fd >= loop->max_events) return -1;

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;

    if (events & AGENTOS_EVENT_TYPE_READ)  ev.events |= EPOLLIN;
    if (events & AGENTOS_EVENT_TYPE_WRITE) ev.events |= EPOLLOUT;

    if (fd < loop->max_events && !loop->fd_entries[fd].level_triggered) {
        ev.events |= EPOLLET;
    }

    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) return -1;

    loop->fd_entries[fd].events = events;
    return 0;
}

void agentos_event_loop_remove_fd(agentos_event_loop_t* loop, int fd) {
    if (!loop || fd < 0 || fd >= loop->max_events) return;
    epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    memset(&loop->fd_entries[fd], 0, sizeof(fd_entry_t));
}

uint64_t agentos_event_loop_add_timer(agentos_event_loop_t* loop, uint64_t interval_ms,
                                       agentos_timer_callback_t cb, void* user_data) {
    if (!loop || !cb) return 0;

    for (int i = 0; i < AGENTOS_EVENT_LOOP_MAX_TIMERS; i++) {
        if (!loop->timers[i].active) {
            loop->timers[i].id = loop->next_timer_id++;
            loop->timers[i].interval_ms = interval_ms;
            loop->timers[i].next_fire_ms = loop->current_time_ms + interval_ms;
            loop->timers[i].cb = cb;
            loop->timers[i].user_data = user_data;
            loop->timers[i].active = true;
            return loop->timers[i].id;
        }
    }
    return 0;
}

int agentos_event_loop_cancel_timer(agentos_event_loop_t* loop, uint64_t timer_id) {
    if (!loop || timer_id == 0) return -1;

    for (int i = 0; i < AGENTOS_EVENT_LOOP_MAX_TIMERS; i++) {
        if (loop->timers[i].active && loop->timers[i].id == timer_id) {
            loop->timers[i].active = false;
            return 0;
        }
    }
    return -1;
}

static void process_timers(agentos_event_loop_t* loop) {
    loop->current_time_ms = get_time_ms();

    for (int i = 0; i < AGENTOS_EVENT_LOOP_MAX_TIMERS; i++) {
        if (loop->timers[i].active && loop->current_time_ms >= loop->timers[i].next_fire_ms) {
            loop->timers[i].cb(loop, loop->timers[i].id, loop->timers[i].user_data);
            if (loop->timers[i].active) {
                loop->timers[i].next_fire_ms = loop->current_time_ms + loop->timers[i].interval_ms;
            }
        }
    }
}

int agentos_event_loop_run(agentos_event_loop_t* loop) {
    if (!loop) return -1;

    loop->running = true;
    loop->stop_requested = false;
    LOG_INFO("Event loop started (max_events=%d)", loop->max_events);

    while (!loop->stop_requested) {
        int timeout_ms = 100;
        int nfds = epoll_wait(loop->epoll_fd, loop->epoll_events, loop->max_events, timeout_ms);

        process_timers(loop);

        for (int i = 0; i < nfds; i++) {
            int fd = loop->epoll_events[i].data.fd;
            uint32_t revents = loop->epoll_events[i].events;

            if (loop->wakeup_fd >= 0 && fd == loop->wakeup_fd) {
                uint64_t val;
                while (read(loop->wakeup_fd, &val, sizeof(val)) > 0) {}
                continue;
            }

            uint32_t user_events = 0;
            if (revents & (EPOLLIN | EPOLLHUP | EPOLLERR)) user_events |= AGENTOS_EVENT_TYPE_READ;
            if (revents & EPOLLOUT) user_events |= AGENTOS_EVENT_TYPE_WRITE;

            if (fd >= 0 && fd < loop->max_events && loop->fd_entries[fd].cb) {
                loop->fd_entries[fd].cb(fd, user_events, loop->fd_entries[fd].user_data);
            } else if (fd >= loop->max_events) {
                LOG_DEBUG("epoll event for fd=%d >= max_events=%d, dropping", fd, loop->max_events);
            }
        }
    }

    loop->running = false;
    LOG_INFO("Event loop stopped");
    return 0;
}

void agentos_event_loop_stop(agentos_event_loop_t* loop) {
    if (!loop) return;
    loop->stop_requested = true;
    if (loop->wakeup_fd >= 0) {
        uint64_t val = 1;
        ssize_t ret = write(loop->wakeup_fd, &val, sizeof(val));
        if (ret < 0) {
            LOG_DEBUG("wakeup write failed: %s", strerror(errno));
        }
    }
}

int agentos_event_loop_wakeup(agentos_event_loop_t* loop) {
    if (!loop || loop->wakeup_fd < 0) return -1;
    uint64_t val = 1;
    if (write(loop->wakeup_fd, &val, sizeof(val)) < 0) return -1;
    return 0;
}

int agentos_event_loop_get_fd_count(agentos_event_loop_t* loop) {
    if (!loop) return 0;
    int count = 0;
    for (int i = 0; i < loop->max_events; i++) {
        if (loop->fd_entries[i].fd > 0) count++;
    }
    return count;
}

#endif
