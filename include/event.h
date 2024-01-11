#ifndef WL_MIRROR_EVENT_H_
#define WL_MIRROR_EVENT_H_

#include <stddef.h>
#include <stdbool.h>
#include <sys/epoll.h>

#include <wayland-util.h>

struct ctx;

typedef struct event_handler {
    struct wl_list link;

    int fd;
    int events;
    int timeout_ms;

    void (*on_event)(struct ctx * ctx);
    void (*on_each)(struct ctx * ctx);
} event_handler_t;

typedef struct ctx_event {
    // epoll fd
    int pollfd;

    // event handlers
    struct wl_list handlers;
} ctx_event_t;

void event_zero(struct ctx * ctx);
void event_init(struct ctx * ctx);
void event_cleanup(struct ctx * ctx);

void event_handler_zero(struct ctx * ctx, event_handler_t * handler);
void event_loop(struct ctx * ctx);

void event_add_fd(struct ctx * ctx, event_handler_t * handler);
void event_change_fd(struct ctx * ctx, event_handler_t * handler);
void event_remove_fd(struct ctx * ctx, event_handler_t * handler);

#endif
