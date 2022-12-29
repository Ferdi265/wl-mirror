#ifndef WL_MIRROR_EVENT_H_
#define WL_MIRROR_EVENT_H_

#include <stddef.h>
#include <stdbool.h>
#include <sys/epoll.h>

struct ctx;

typedef struct event_handler {
    struct event_handler * next;
    int fd;
    int events;
    int timeout_ms;
    void (*on_event)(struct ctx * ctx);
    void (*on_each)(struct ctx * ctx);
} event_handler_t;

typedef struct ctx_event {
    int pollfd;
    event_handler_t * handlers;

    bool initialized;
} ctx_event_t;

void init_event(struct ctx * ctx);
void cleanup_event(struct ctx * ctx);

void event_add_fd(struct ctx * ctx, event_handler_t * handler);
void event_change_fd(struct ctx * ctx, event_handler_t * handler);
void event_remove_fd(struct ctx * ctx, event_handler_t * handler);
void event_loop(struct ctx * ctx);

#endif
