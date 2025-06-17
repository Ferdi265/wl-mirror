#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <wlm/context.h>
#include <wlm/event.h>
#include <wlm/event/emit.h>

static void add_handler(ctx_t * ctx, wlm_event_loop_handler_t * handler) {
    handler->next = ctx->event.handlers;
    ctx->event.handlers = handler;
}

static void remove_handler(ctx_t * ctx, int fd) {
    wlm_event_loop_handler_t ** pcur = &ctx->event.handlers;
    wlm_event_loop_handler_t * cur = *pcur;
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
    wlm_event_loop_handler_t * cur = ctx->event.handlers;
    while (cur != NULL) {
        if (cur->on_each != NULL) {
            cur->on_each(ctx);
        }

        cur = cur->next;
    }
}


static wlm_event_loop_handler_t * min_timeout(ctx_t * ctx) {
    wlm_event_loop_handler_t * min = NULL;
    wlm_event_loop_handler_t * cur = ctx->event.handlers;
    while (cur != NULL) {
        if (cur->timeout_ms != -1) {
            if (min == NULL || cur->timeout_ms < min->timeout_ms) min = cur;
        }

        cur = cur->next;
    }

    return min;
}

void wlm_event_add_fd(ctx_t * ctx, wlm_event_loop_handler_t * handler) {
    struct epoll_event event;
    event.events = handler->events;
    event.data.ptr = handler;

    if (epoll_ctl(ctx->event.pollfd, EPOLL_CTL_ADD, handler->fd, &event) == -1) {
        wlm_log_error("event::add_fd(): failed to add fd to epoll instance\n");
        wlm_exit_fail(ctx);
    }

    add_handler(ctx, handler);
}

void wlm_event_change_fd(ctx_t * ctx, wlm_event_loop_handler_t * handler) {
    struct epoll_event event;
    event.events = handler->events;
    event.data.ptr = handler;

    if (epoll_ctl(ctx->event.pollfd, EPOLL_CTL_MOD, handler->fd, &event) == -1) {
        wlm_log_error("event::change_fd(): failed to modify fd in epoll instance\n");
        wlm_exit_fail(ctx);
    }
}

void wlm_event_remove_fd(ctx_t * ctx, wlm_event_loop_handler_t * handler) {
    if (epoll_ctl(ctx->event.pollfd, EPOLL_CTL_DEL, handler->fd, NULL) == -1) {
        wlm_log_error("event::remove_fd(): failed to remove fd from epoll instance\n");
    }

    remove_handler(ctx, handler->fd);
    handler->next = NULL;
}

#define MAX_EVENTS 10
void wlm_event_loop(ctx_t * ctx) {
    struct epoll_event events[MAX_EVENTS];
    int num_events;

    wlm_event_loop_handler_t * timeout_handler;
    int timeout_ms;

    timeout_handler = min_timeout(ctx);
    timeout_ms = timeout_handler == NULL ? -1 : timeout_handler->timeout_ms;

    while ((num_events = epoll_wait(ctx->event.pollfd, events, MAX_EVENTS, timeout_ms)) != -1 && !wlm_wayland_core_is_closing(ctx)) {
        for (int i = 0; i < num_events; i++) {
            wlm_event_loop_handler_t * handler = (wlm_event_loop_handler_t *)events[i].data.ptr;
            handler->on_event(ctx, events[i].events);
        }

        if (num_events == 0 && timeout_handler != NULL) {
            timeout_handler->on_event(ctx, 0);
        }

        wlm_event_emit_before_poll(ctx);

        timeout_handler = min_timeout(ctx);
        timeout_ms = timeout_handler == NULL ? -1 : timeout_handler->timeout_ms;
        call_each_handler(ctx);
    }
}

void wlm_event_init(ctx_t * ctx) {
    ctx->event.pollfd = epoll_create(1);
    if (ctx->event.pollfd == -1) {
        wlm_log_error("event::init(): failed to create epoll instance\n");
        wlm_exit_fail(ctx);
        return;
    }

    ctx->event.handlers = NULL;
    ctx->event.initialized = true;
}

void wlm_event_cleanup(ctx_t * ctx) {
    close(ctx->event.pollfd);
}
