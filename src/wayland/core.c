#include <wlm/context.h>

// --- event loop handlers ---

static void on_loop_event(ctx_t * ctx) {
    if (wl_display_dispatch(ctx->wl.core.display) == -1) {
        ctx->wl.core.closing = true;
    }
}

// --- libdecor event handlers ---

static ctx_t * libdecor_error_context;
static void on_libdecor_error(
    struct libdecor * libdecor_context,
    enum libdecor_error error, const char * message
) {
    if (libdecor_context == NULL) return;

    ctx_t * ctx = libdecor_error_context;

    log_error("wayland::core::on_libdecor_error(): error %d, %s\n", error, message);
    wlm_exit_fail(ctx);
}

static struct libdecor_interface libdecor_listener = {
    .error = on_libdecor_error
};

// --- internal event handlers ---

void wlm_wayland_core_on_before_poll(ctx_t * ctx) {
    wl_display_flush(ctx->wl.core.display);
}

// --- initialization and cleanup ---

void wlm_wayland_core_zero(ctx_t * ctx) {
    // wayland display
    ctx->wl.core.display = NULL;

    // libdecor context
    ctx->wl.core.libdecor_context = NULL;

    // program state
    ctx->wl.core.closing = false;

    // event loop handler
    wlm_event_loop_handler_zero(ctx, &ctx->wl.core.event_handler);
    ctx->wl.core.event_handler.events = EPOLLIN;
    ctx->wl.core.event_handler.on_event = on_loop_event;
}

void wlm_wayland_core_init(ctx_t * ctx) {
    // connect to wayland display
    ctx->wl.core.display = wl_display_connect(NULL);
    if (ctx->wl.core.display == NULL) {
        log_error("wayland::core::init(): failed to connect to wayland display\n");
        wlm_exit_fail(ctx);
    }

    // initialize libdecor
    libdecor_error_context = ctx;
    ctx->wl.core.libdecor_context = libdecor_new(ctx->wl.core.display, &libdecor_listener);

    // add fd to event loop
    ctx->wl.core.event_handler.fd = wl_display_get_fd(ctx->wl.core.display);
    wlm_event_loop_add_fd(ctx, &ctx->wl.core.event_handler);
}

void wlm_wayland_core_cleanup(ctx_t * ctx) {
    // remove fd from event loop
    if (ctx->wl.core.event_handler.fd != -1) wlm_event_loop_remove_fd(ctx, &ctx->wl.core.event_handler);

    // destroy libdecor context
    if (ctx->wl.core.libdecor_context != NULL) libdecor_unref(ctx->wl.core.libdecor_context);

    // disconnect from wayland display
    if (ctx->wl.core.display != NULL) wl_display_disconnect(ctx->wl.core.display);

    wlm_wayland_core_zero(ctx);
}

// --- public functions ---

bool wlm_wayland_core_is_closing(ctx_t * ctx) {
    return ctx->wl.core.closing;
}
