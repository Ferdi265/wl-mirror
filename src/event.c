#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "context.h"
#include "event.h"

static void add_handler(ctx_t * ctx, event_handler_t * handler) {
    handler->next = ctx->event.handlers;
    ctx->event.handlers = handler;
}

static void remove_handler(ctx_t * ctx, int fd) {
    event_handler_t ** pcur = &ctx->event.handlers;
    event_handler_t * cur = *pcur;
    while (cur != NULL) {
        if (cur->fd == fd) {
            *pcur = cur->next;
            return;
        }

        pcur = &cur->next;
        cur = cur->next;
    }
}

static void call_each_handler(ctx_t * ctx) {
    event_handler_t * cur = ctx->event.handlers;
    while (cur != NULL) {
        if (cur->on_each != NULL) {
            cur->on_each(ctx);
        }

        cur = cur->next;
    }
}


static event_handler_t * min_timeout(ctx_t * ctx) {
    event_handler_t * min = NULL;
    event_handler_t * cur = ctx->event.handlers;
    while (cur != NULL) {
        if (cur->timeout_ms != -1) {
            if (min == NULL || cur->timeout_ms < min->timeout_ms) min = cur;
        }

        cur = cur->next;
    }

    return min;
}

void event_add_fd(ctx_t * ctx, event_handler_t * handler) {
    struct epoll_event event;
    event.events = handler->events;
    event.data.ptr = handler;

    if (epoll_ctl(ctx->event.pollfd, EPOLL_CTL_ADD, handler->fd, &event) == -1) {
        log_error("event::add_fd(): failed to add fd to epoll instance\n");
        exit_fail(ctx);
    }

    add_handler(ctx, handler);
}

void event_change_fd(ctx_t * ctx, event_handler_t * handler) {
    struct epoll_event event;
    event.events = handler->events;
    event.data.ptr = handler;

    if (epoll_ctl(ctx->event.pollfd, EPOLL_CTL_MOD, handler->fd, &event) == -1) {
        log_error("event::change_fd(): failed to modify fd in epoll instance\n");
        exit_fail(ctx);
    }
}

void event_remove_fd(ctx_t * ctx, event_handler_t * handler) {
    if (epoll_ctl(ctx->event.pollfd, EPOLL_CTL_DEL, handler->fd, NULL) == -1) {
        log_error("event::remove_fd(): failed to remove fd from epoll instance\n");
        exit_fail(ctx);
    }

    remove_handler(ctx, handler->fd);
    handler->next = NULL;
}

#define MAX_EVENTS 10
void event_loop(ctx_t * ctx) {
    struct epoll_event events[MAX_EVENTS];
    int num_events;

    event_handler_t * timeout_handler;
    int timeout_ms;

    timeout_handler = min_timeout(ctx);
    timeout_ms = timeout_handler == NULL ? -1 : timeout_handler->timeout_ms;

    while ((num_events = epoll_wait(ctx->event.pollfd, events, MAX_EVENTS, timeout_ms)) != -1 && !ctx->wl.closing) {
        for (int i = 0; i < num_events; i++) {
            event_handler_t * handler = (event_handler_t *)events[i].data.ptr;
            handler->on_event(ctx);
        }

        if (num_events == 0 && timeout_handler != NULL) {
            timeout_handler->on_event(ctx);
        }

        timeout_handler = min_timeout(ctx);
        timeout_ms = timeout_handler == NULL ? -1 : timeout_handler->timeout_ms;
        call_each_handler(ctx);
    }
}

void init_event(ctx_t * ctx) {
    ctx->event.pollfd = epoll_create(1);
    if (ctx->event.pollfd == -1) {
        log_error("event::init(): failed to create epoll instance\n");
        exit_fail(ctx);
        return;
    }

    ctx->event.handlers = NULL;
    ctx->event.initialized = true;
}

void cleanup_event(ctx_t * ctx) {
    close(ctx->event.pollfd);
}
