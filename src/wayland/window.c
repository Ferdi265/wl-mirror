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
    .ping = on_xdg_wm_base_ping,
};

// --- surface event handlers ---

static void on_surface_enter(
    void * data, struct wl_surface * surface,
    struct wl_output * output
) {
    if (surface == NULL) return;
    if (!wayland_registry_is_own_proxy((struct wl_proxy *)output)) return;

    ctx_t * ctx = (ctx_t *)data;
    wayland_output_entry_t * entry = wayland_output_find(ctx, output);

    log_debug(ctx, "wayland::window::on_surface_enter(): entering output %s\n", entry->name);

    if (ctx->wl.window.current_output != entry) {
        ctx->wl.window.current_output = entry;
        // TODO: react to change? emit something?
    }
}

static void on_surface_leave(
    void * data, struct wl_surface * surface,
    struct wl_output * output
) {
    if (surface == NULL) return;
    if (!wayland_registry_is_own_proxy((struct wl_proxy *)output)) return;

    ctx_t * ctx = (ctx_t *)data;
    wayland_output_entry_t * entry = wayland_output_find(ctx, output);

    log_debug(ctx, "wayland::window::on_surface_leave(): leaving output %s\n", entry->name);

    // ignore: current output is set with next entered output anyway
}

static void on_surface_preferred_buffer_scale(
    void * data, struct wl_surface * surface,
    int32_t scale
) {
    if (surface == NULL) return;

    ctx_t * ctx = (ctx_t *)data;

    log_debug(ctx, "wayland::window::on_surface_preferred_buffer_scale(): preferred integer scale = %d\n", scale);

    // TODO: react to scale
}

static void on_surface_preferred_buffer_transform(
    void * data, struct wl_surface * surface,
    uint32_t transform_int
) {
    if (surface == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    enum wl_output_transform transform = (enum wl_output_transform)transform_int;

    log_debug(ctx, "wayland::window::on_surface_preferred_buffer_transform(): preferred transform = %s\n",
        PRINT_WL_OUTPUT_TRANSFORM(transform)
    );

    // TODO: react to transform
    (void)transform;
}

static const struct wl_surface_listener surface_listener = {
    .enter = on_surface_enter,
    .leave = on_surface_leave,
    .preferred_buffer_scale = on_surface_preferred_buffer_scale,
    .preferred_buffer_transform = on_surface_preferred_buffer_transform,
};

// --- fractional_scale event handlers ---

static void on_fractional_scale_preferred_scale(
    void * data, struct wp_fractional_scale_v1 * fractional_scale,
    uint32_t scale_times_120
) {
    if (fractional_scale == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    double scale = scale_times_120 / 120.;

    log_debug(ctx, "wayland::window::on_fractional_scale_preferred_scale(): preferred fractional scale = %.3f\n", scale);

    // TODO: react to scale
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
    .preferred_scale = on_fractional_scale_preferred_scale,
};

// --- xdg_surface event handlers ---

static void on_xdg_surface_configure(
    void * data, struct xdg_surface * xdg_surface,
    uint32_t serial
) {
    if (xdg_surface == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    log_debug(ctx, "wayland::window::on_xdg_surface_configure(): serial = %d\n", serial);

    // TODO
    (void)ctx;
    (void)serial;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = on_xdg_surface_configure,
};

// --- xdg_toplevel event handlers ---

static void on_xdg_toplevel_configure(
    void * data, struct xdg_toplevel * xdg_toplevel,
    int32_t width, int32_t height, struct wl_array * states
) {
    if (xdg_toplevel == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    log_debug(ctx, "wayland::window::on_xdg_toplevel_configure(): width = %d, height = %d\n", width, height);

    // TODO
    (void)ctx;
    (void)width;
    (void)height;
    (void)states;
}

static void on_xdg_toplevel_configure_bounds(
    void * data, struct xdg_toplevel * xdg_toplevel,
    int32_t width, int32_t height
) {
    if (xdg_toplevel == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    log_debug(ctx, "wayland::window::on_xdg_toplevel_configure_bounds(): width = %d, height = %d\n", width, height);

    // TODO
    (void)ctx;
    (void)width;
    (void)height;
}

static void on_xdg_toplevel_wm_capabilities(
    void * data, struct xdg_toplevel * xdg_toplevel,
    struct wl_array * capabilities
) {
    if (xdg_toplevel == NULL) return;

    ctx_t * ctx = (ctx_t *)data;

    // TODO
    (void)ctx;
    (void)capabilities;
}

static void on_xdg_toplevel_close(
    void * data, struct xdg_toplevel * xdg_toplevel
) {
    if (xdg_toplevel == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    log_debug(ctx, "wayland::window::on_xdg_toplevel_close(): close requested\n");

    ctx->wl.core.closing = true;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = on_xdg_toplevel_configure,
    .configure_bounds = on_xdg_toplevel_configure_bounds,
    .wm_capabilities = on_xdg_toplevel_wm_capabilities,
    .close = on_xdg_toplevel_close,
};

// --- initialization and cleanup ---

void wayland_window_zero(ctx_t * ctx) {
    ctx->wl.window.surface = NULL;
    ctx->wl.window.viewport = NULL;
    ctx->wl.window.fractional_scale = NULL;
    ctx->wl.window.xdg_surface = NULL;
    ctx->wl.window.xdg_toplevel = NULL;

    ctx->wl.window.current_output = NULL;
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
