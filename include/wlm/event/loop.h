#ifndef WL_MIRROR_EVENT_LOOP_H_
#define WL_MIRROR_EVENT_LOOP_H_

#include <stddef.h>
#include <stdbool.h>
#include <sys/epoll.h>

#include <wayland-util.h>

struct ctx;

typedef struct {
    struct wl_list link;

    int fd;
    int events;
    int timeout_ms;

    void (*on_event)(struct ctx * ctx);
    void (*on_each)(struct ctx * ctx);
} wlm_event_loop_handler_t;

typedef struct ctx_event {
    // epoll fd
    int pollfd;

    // event handlers
    struct wl_list handlers;
} ctx_event_loop_t;

#define EVENT_LOOP_MAX_EVENTS 10

void wlm_event_loop_zero(struct ctx * ctx);
void wlm_event_loop_init(struct ctx * ctx);
void wlm_event_loop_cleanup(struct ctx * ctx);

void wlm_event_loop_handler_zero(struct ctx * ctx, wlm_event_loop_handler_t * handler);
void wlm_event_loop_run(struct ctx * ctx);

void wlm_event_loop_add_fd(struct ctx * ctx, wlm_event_loop_handler_t * handler);
void wlm_event_loop_change_fd(struct ctx * ctx, wlm_event_loop_handler_t * handler);
void wlm_event_loop_remove_fd(struct ctx * ctx, wlm_event_loop_handler_t * handler);

#endif
