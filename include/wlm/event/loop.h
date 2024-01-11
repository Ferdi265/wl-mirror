#ifndef WL_MIRROR_EVENT_LOOP_H_
#define WL_MIRROR_EVENT_LOOP_H_

#include <stddef.h>
#include <stdbool.h>
#include <sys/epoll.h>

#include <wayland-util.h>

typedef struct ctx ctx_t;

typedef struct {
    struct wl_list link;

    int fd;
    int events;
    int timeout_ms;

    void (*on_event)(ctx_t *);
} wlm_event_loop_handler_t;

typedef struct ctx_event {
    // epoll fd
    int pollfd;

    // event handlers
    struct wl_list handlers;
} ctx_event_loop_t;

#define EVENT_LOOP_MAX_EVENTS 10

void wlm_event_loop_zero(ctx_t *);
void wlm_event_loop_init(ctx_t *);
void wlm_event_loop_cleanup(ctx_t *);

void wlm_event_loop_handler_zero(ctx_t *, wlm_event_loop_handler_t * handler);
void wlm_event_loop_run(ctx_t *);

void wlm_event_loop_add_fd(ctx_t *, wlm_event_loop_handler_t * handler);
void wlm_event_loop_change_fd(ctx_t *, wlm_event_loop_handler_t * handler);
void wlm_event_loop_remove_fd(ctx_t *, wlm_event_loop_handler_t * handler);

#endif
