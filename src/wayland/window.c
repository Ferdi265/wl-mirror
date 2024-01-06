#include "context.h"

// --- xdg_wm_base event handlers ---

static void on_xdg_wm_base_ping(
    void * data, struct xdg_wm_base * xdg_wm_base,
    uint32_t serial
) {
    if (xdg_wm_base == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    xdg_wm_base_pong(ctx->wl.protocols.xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = on_xdg_wm_base_ping
};

// --- surface event handlers ---

static const struct wl_surface_listener surface_listener = {
    // TODO
};

// --- fractional_scale event handlers ---

static const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
    // TODO
};

// --- xdg_surface event handlers ---

static const struct xdg_surface_listener xdg_surface_listener = {
    // TODO
};

// --- xdg_toplevel event handlers ---

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    // TODO
};

// --- initialization and cleanup ---

void wayland_window_zero(ctx_t * ctx) {
    ctx->wl.window.surface = NULL;
    ctx->wl.window.viewport = NULL;
    ctx->wl.window.fractional_scale = NULL;
    ctx->wl.window.xdg_surface = NULL;
    ctx->wl.window.xdg_toplevel = NULL;
}

void wayland_window_init(ctx_t * ctx) {
    (void)ctx;
}

void wayland_window_cleanup(ctx_t * ctx) {
    if (ctx->wl.window.xdg_toplevel != NULL) xdg_toplevel_destroy(ctx->wl.window.xdg_toplevel);
    if (ctx->wl.window.xdg_surface != NULL) xdg_surface_destroy(ctx->wl.window.xdg_surface);
    if (ctx->wl.window.fractional_scale != NULL) wp_fractional_scale_v1_destroy(ctx->wl.window.fractional_scale);
    if (ctx->wl.window.viewport != NULL) wp_viewport_destroy(ctx->wl.window.viewport);
    if (ctx->wl.window.surface != NULL) wl_surface_destroy(ctx->wl.window.surface);

    wayland_window_zero(ctx);
}

// --- internal event handlers ---

void wayland_window_on_registry_initial_sync(ctx_t * ctx) {
    log_debug(ctx, "wayland::window::on_registry_initial_sync(): creating window\n");

    // listen for ping events
    xdg_wm_base_add_listener(ctx->wl.protocols.xdg_wm_base, &xdg_wm_base_listener, (void *)ctx);

    // create surface
    ctx->wl.window.surface = wl_compositor_create_surface(ctx->wl.protocols.compositor);
    wl_surface_add_listener(ctx->wl.window.surface, &surface_listener, (void *)ctx);

    // create viewport
    ctx->wl.window.viewport = wp_viewporter_get_viewport(ctx->wl.protocols.viewporter, ctx->wl.window.surface);

    // create fractional scale if supported
    if (ctx->wl.protocols.fractional_scale_manager != NULL) {
        ctx->wl.window.fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(
            ctx->wl.protocols.fractional_scale_manager, ctx->wl.window.surface
        );
        wp_fractional_scale_v1_add_listener(ctx->wl.window.fractional_scale, &fractional_scale_listener, (void *)ctx);
    }

    // create xdg surface
    ctx->wl.window.xdg_surface = xdg_wm_base_get_xdg_surface(ctx->wl.protocols.xdg_wm_base, ctx->wl.window.surface);
    xdg_surface_add_listener(ctx->wl.window.xdg_surface, &xdg_surface_listener, (void *)ctx);

    // create xdg toplevel
    ctx->wl.window.xdg_toplevel = xdg_surface_get_toplevel(ctx->wl.window.xdg_surface);
    xdg_toplevel_add_listener(ctx->wl.window.xdg_toplevel, &xdg_toplevel_listener, (void *)ctx);

    // set xdg toplevel properties
    xdg_toplevel_set_app_id(ctx->wl.window.xdg_toplevel, "at.yrlf.wl_mirror");
    xdg_toplevel_set_title(ctx->wl.window.xdg_toplevel, "Wayland Output Mirror");

    // don't commit surface yet, wait for outputs
}

void wayland_window_on_output_initial_sync(ctx_t * ctx) {
    // TODO: set fullscreen here if already requested

    // commit surface
    log_debug(ctx, "wayland::window::on_output_initial_sync(): committing surface\n");
    wl_surface_commit(ctx->wl.window.surface);
}
