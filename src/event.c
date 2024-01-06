#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "context.h"
#include "event.h"

static void call_each_handler(ctx_t * ctx) {
    event_handler_t * cur;
    wl_list_for_each(cur, &ctx->event.handlers, link) {
        if (cur->on_each != NULL) cur->on_each(ctx);
    }
}

static event_handler_t * min_timeout(ctx_t * ctx) {
    event_handler_t * min = NULL;
    event_handler_t * cur;
    wl_list_for_each(cur, &ctx->event.handlers, link) {
        if (cur->timeout_ms == -1) continue;
        if (min == NULL || cur->timeout_ms < min->timeout_ms) min = cur;
    }

    return min;
}

void event_zero(ctx_t * ctx) {
    // epoll fd
    ctx->event.pollfd = -1;

    // event handlers
    wl_list_init(&ctx->event.handlers);
}

void event_init(ctx_t * ctx) {
    // initialize epoll fd
    ctx->event.pollfd = epoll_create(1);
    if (ctx->event.pollfd == -1) {
        log_error("event::init(): failed to create epoll instance: %s\n", strerror(errno));
        exit_fail(ctx);
        return;
    }
}

void event_cleanup(ctx_t * ctx) {
    // epoll fd
    if (ctx->event.pollfd != -1) close(ctx->event.pollfd);

    // event handlers are owned by their respective subsystems

    event_zero(ctx);
}

void event_handler_zero(struct ctx * ctx, event_handler_t * handler) {
    wl_list_init(&handler->link);

    handler->fd = -1;
    handler->events = 0;
    handler->timeout_ms = -1;

    handler->on_event = NULL;
    handler->on_each = NULL;

    (void)ctx;
}

#define MAX_EVENTS 10
void event_loop(ctx_t * ctx) {
    struct epoll_event events[MAX_EVENTS];
    int num_events;

    event_handler_t * timeout_handler;
    int timeout_ms;

    call_each_handler(ctx);
    timeout_handler = min_timeout(ctx);
    timeout_ms = timeout_handler == NULL ? -1 : timeout_handler->timeout_ms;

    while ((num_events = epoll_wait(ctx->event.pollfd, events, MAX_EVENTS, timeout_ms)) != -1 && !wayland_core_is_closing(ctx)) {
        for (int i = 0; i < num_events; i++) {
            event_handler_t * handler = (event_handler_t *)events[i].data.ptr;
            handler->on_event(ctx);
        }

        if (num_events == 0 && timeout_handler != NULL) {
            timeout_handler->on_event(ctx);
        }

        call_each_handler(ctx);
        timeout_handler = min_timeout(ctx);
        timeout_ms = timeout_handler == NULL ? -1 : timeout_handler->timeout_ms;
    }
}

void event_add_fd(ctx_t * ctx, event_handler_t * handler) {
    struct epoll_event event;
    event.events = handler->events;
    event.data.ptr = handler;

    if (epoll_ctl(ctx->event.pollfd, EPOLL_CTL_ADD, handler->fd, &event) == -1) {
        log_error("event::add_fd(): failed to add fd to epoll instance: %s\n", strerror(errno));
        exit_fail(ctx);
    }

    wl_list_insert(&ctx->event.handlers, &handler->link);
}

void event_change_fd(ctx_t * ctx, event_handler_t * handler) {
    struct epoll_event event;
    event.events = handler->events;
    event.data.ptr = handler;

    if (epoll_ctl(ctx->event.pollfd, EPOLL_CTL_MOD, handler->fd, &event) == -1) {
        log_error("event::change_fd(): failed to modify fd in epoll instance: %s\n", strerror(errno));
        exit_fail(ctx);
    }
}

void event_remove_fd(ctx_t * ctx, event_handler_t * handler) {
    if (epoll_ctl(ctx->event.pollfd, EPOLL_CTL_DEL, handler->fd, NULL) == -1) {
        log_error("event::remove_fd(): failed to remove fd from epoll instance: %s\n", strerror(errno));
    }

    wl_list_remove(&handler->link);
}
