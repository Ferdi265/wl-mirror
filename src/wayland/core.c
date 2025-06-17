#include <wlm/wayland/core.h>
#include <wlm/context.h>
#include <string.h>

// --- event loop handlers ---

static void on_loop_event(ctx_t * ctx, uint32_t events) {
    if (wl_display_dispatch(ctx->wl.core.display) == -1) {
        ctx->wl.core.closing = true;
    }

    (void)events;
}

// --- libdecor event handlers ---

#if WITH_LIBDECOR
static ctx_t * libdecor_error_context;
static void on_libdecor_error(
    struct libdecor * libdecor_context,
    enum libdecor_error error, const char * message
) {
    if (libdecor_context == NULL) return;

    ctx_t * ctx = libdecor_error_context;

    wlm_log_error("wayland::core: libdecor error %d, %s\n", error, message);
    wlm_exit_fail(ctx);
}

static struct libdecor_interface libdecor_listener = {
    .error = on_libdecor_error
};
#endif

// --- internal event handlers ---

void wlm_wayland_core_on_before_poll(ctx_t * ctx) {
    wl_display_flush(ctx->wl.core.display);
}

// --- initialization and cleanup ---

void wlm_wayland_core_zero(ctx_t * ctx) {
    // zero everything
    memset(&ctx->wl.core, 0, sizeof ctx->wl.core);

    ctx->wl.core.event_handler.fd = -1;
    ctx->wl.core.event_handler.timeout_ms = -1;
    ctx->wl.core.event_handler.events = EPOLLIN;
    ctx->wl.core.event_handler.on_event = on_loop_event;
}

void wlm_wayland_core_init(ctx_t * ctx) {
    ctx->wl.core.init_called = true;

    // connect to wayland display
    ctx->wl.core.display = wl_display_connect(NULL);
    if (ctx->wl.core.display == NULL) {
        wlm_log_error("wayland::core: failed to connect to wayland display\n");
        wlm_exit_fail(ctx);
    }

#if WITH_LIBDECOR
    // initialize libdecor
    libdecor_error_context = ctx;
    ctx->wl.core.libdecor_context = libdecor_new(ctx->wl.core.display, &libdecor_listener);
#endif

    // add fd to event loop
    ctx->wl.core.event_handler.fd = wl_display_get_fd(ctx->wl.core.display);
    wlm_event_add_fd(ctx, &ctx->wl.core.event_handler);

    ctx->wl.core.init_done = true;
}

void wlm_wayland_core_cleanup(ctx_t * ctx) {
    if (ctx->wl.core.event_handler.fd != -1) {
        wlm_event_remove_fd(ctx, &ctx->wl.core.event_handler);
        ctx->wl.core.event_handler.fd = -1;
    }

#if WITH_LIBDECOR
    if (ctx->wl.core.libdecor_context != NULL) {
        libdecor_unref(ctx->wl.core.libdecor_context);
        ctx->wl.libdecor_context = NULL;
    }
#endif

    if (ctx->wl.core.display != NULL) {
        wl_display_disconnect(ctx->wl.core.display);
        ctx->wl.core.display = NULL;
    }

    ctx->wl.core.init_called = false;
    ctx->wl.core.init_done = false;
}

// --- public functions ---

void wlm_wayland_core_request_close(ctx_t * ctx) {
    ctx->wl.core.closing = true;
}

// --- getters ---

struct wl_display * wlm_wayland_core_get_display(ctx_t * ctx) {
    return ctx->wl.core.display;
}

#ifdef WITH_LIBDECOR
struct libdecor * wlm_wayland_core_get_libdecor_context(ctx_t * ctx) {
    return ctx->wl.core.libdecor_context;
}
#endif

bool wlm_wayland_core_is_closing(ctx_t * ctx) {
    return ctx->wl.core.closing;
}

bool wlm_wayland_core_is_init_called(ctx_t * ctx) {
    return ctx->wl.core.init_called;
}

bool wlm_wayland_core_is_init_done(ctx_t * ctx) {
    return ctx->wl.core.init_done;
}
