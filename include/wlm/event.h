#ifndef WL_MIRROR_EVENT_H_
#define WL_MIRROR_EVENT_H_

#include <stddef.h>
#include <stdbool.h>
#include <sys/epoll.h>

struct ctx;

typedef struct wlm_event_loop_handler {
    struct wlm_event_loop_handler * next;
    int fd;
    int events;
    int timeout_ms;
    void (*on_event)(struct ctx * ctx, uint32_t events);
    void (*on_each)(struct ctx * ctx);
} wlm_event_loop_handler_t;

typedef struct ctx_event {
    int pollfd;
    wlm_event_loop_handler_t * handlers;

    bool initialized;
} ctx_event_t;

void wlm_event_init(struct ctx * ctx);
void wlm_event_cleanup(struct ctx * ctx);

void wlm_event_add_fd(struct ctx * ctx, wlm_event_loop_handler_t * handler);
void wlm_event_change_fd(struct ctx * ctx, wlm_event_loop_handler_t * handler);
void wlm_event_remove_fd(struct ctx * ctx, wlm_event_loop_handler_t * handler);
void wlm_event_loop(struct ctx * ctx);

#endif
