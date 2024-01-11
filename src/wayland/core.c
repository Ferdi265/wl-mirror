#include "context.h"
#include "wayland.h"

static void on_loop_event(ctx_t * ctx) {
    if (wl_display_dispatch(ctx->wl.core.display) == -1) {
        ctx->wl.core.closing = true;
    }
}

static void on_loop_each(ctx_t * ctx) {
    wl_display_flush(ctx->wl.core.display);
}

void wayland_core_zero(ctx_t * ctx) {
    // wayland display
    ctx->wl.core.display = NULL;
    ctx->wl.core.closing = false;

    // event loop handler
    event_handler_zero(ctx, &ctx->wl.core.event_handler);
    ctx->wl.core.event_handler.events = EPOLLIN;
    ctx->wl.core.event_handler.on_event = on_loop_event;
    ctx->wl.core.event_handler.on_each = on_loop_each;
}

void wayland_core_init(ctx_t * ctx) {
    // connect to wayland display
    ctx->wl.core.display = wl_display_connect(NULL);
    if (ctx->wl.core.display == NULL) {
        log_error("wayland::core::init(): failed to connect to wayland display\n");
        exit_fail(ctx);
    }

    // add fd to event loop
    ctx->wl.core.event_handler.fd = wl_display_get_fd(ctx->wl.core.display);
    event_add_fd(ctx, &ctx->wl.core.event_handler);
}

void wayland_core_cleanup(ctx_t * ctx) {
    // remove fd from event loop
    if (ctx->wl.core.event_handler.fd != -1) event_remove_fd(ctx, &ctx->wl.core.event_handler);

    // disconnect from wayland display
    if (ctx->wl.core.display != NULL) wl_display_disconnect(ctx->wl.core.display);

    wayland_core_zero(ctx);
}

bool wayland_core_is_closing(ctx_t * ctx) {
    return ctx->wl.core.closing;
}
