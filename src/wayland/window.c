#include <wlm/context.h>

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
    if (!wlm_wayland_registry_is_own_proxy((struct wl_proxy *)output)) return;

    ctx_t * ctx = (ctx_t *)data;
    wlm_wayland_output_entry_t * entry = wlm_wayland_output_find(ctx, output);

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
    if (!wlm_wayland_registry_is_own_proxy((struct wl_proxy *)output)) return;

    ctx_t * ctx = (ctx_t *)data;
    wlm_wayland_output_entry_t * entry = wlm_wayland_output_find(ctx, output);

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
        WLM_PRINT_OUTPUT_TRANSFORM(transform)
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

// --- libdecor_frame event handlers ---

static void on_libdecor_frame_configure(
    struct libdecor_frame * frame,
    struct libdecor_configuration * configuration, void * data
) {
    if (frame == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    log_debug(ctx, "wayland::window::on_libdecor_frame_configure(): configuring\n");

    int width = 0;
    int height = 0;
    if (!libdecor_configuration_get_content_size(configuration, frame, &width, &height)) {
        if (ctx->wl.window.width == 0 || ctx->wl.window.height == 0) {
            log_debug(ctx, "wayland::window::on_libdecor_frame_configure(): falling back to default size\n");
            width = DEFAULT_WIDTH;
            height = DEFAULT_HEIGHT;
        } else {
            log_debug(ctx, "wayland::window::on_libdecor_frame_configure(): falling back to previous size\n");
            width = ctx->wl.window.width;
            height = ctx->wl.window.height;
        }
    }

    // check fullscreen state
    bool is_fullscreen = false;
    enum libdecor_window_state window_state;
    if (libdecor_configuration_get_window_state(configuration, &window_state)) {
        if (window_state & LIBDECOR_WINDOW_STATE_FULLSCREEN) {
            is_fullscreen = true;
        }
    }

    struct libdecor_state * state = libdecor_state_new(width, height);
    libdecor_frame_commit(frame, state, configuration);
    libdecor_state_free(state);

    // TODO: check if things changed, emit events
    (void)is_fullscreen;
}

static void on_libdecor_frame_commit(
    struct libdecor_frame * frame, void * data
) {
    (void)frame;
    (void)data;
}

static void on_libdecor_frame_close(
    struct libdecor_frame * frame, void * data
) {
    if (frame == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    log_debug(ctx, "wayland::window::on_libdecor_frame_close(): close requested\n");

    ctx->wl.core.closing = true;
}

static void on_libdecor_frame_dismiss_popup(
    struct libdecor_frame * frame,
    const char * seat_name, void * data
) {
    (void)frame;
    (void)seat_name;
    (void)data;
}

static struct libdecor_frame_interface libdecor_frame_listener = {
    .configure = on_libdecor_frame_configure,
    .commit = on_libdecor_frame_commit,
    .close = on_libdecor_frame_close,
    .dismiss_popup = on_libdecor_frame_dismiss_popup
};

// --- initialization and cleanup ---

void wlm_wayland_window_zero(ctx_t * ctx) {
    ctx->wl.window.surface = NULL;
    ctx->wl.window.viewport = NULL;
    ctx->wl.window.fractional_scale = NULL;
    ctx->wl.window.libdecor_frame = NULL;

    ctx->wl.window.current_output = NULL;
}

void wlm_wayland_window_init(ctx_t * ctx) {
    (void)ctx;
}

void wlm_wayland_window_cleanup(ctx_t * ctx) {
    if (ctx->wl.window.libdecor_frame != NULL) libdecor_frame_unref(ctx->wl.window.libdecor_frame);
    if (ctx->wl.window.fractional_scale != NULL) wp_fractional_scale_v1_destroy(ctx->wl.window.fractional_scale);
    if (ctx->wl.window.viewport != NULL) wp_viewport_destroy(ctx->wl.window.viewport);
    if (ctx->wl.window.surface != NULL) wl_surface_destroy(ctx->wl.window.surface);

    wlm_wayland_window_zero(ctx);
}

// --- internal event handlers ---

void wlm_wayland_window_on_registry_initial_sync(ctx_t * ctx) {
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
    ctx->wl.window.libdecor_frame = libdecor_decorate(ctx->wl.core.libdecor_context, ctx->wl.window.surface, &libdecor_frame_listener, (void *)ctx);

    // don't map frame or commit surface yet, wait for outputs
}

void wlm_wayland_window_on_output_initial_sync(ctx_t * ctx) {
    // TODO: set fullscreen here if already requested

    // set libdecor app properties
    libdecor_frame_set_app_id(ctx->wl.window.libdecor_frame, "at.yrlf.wl_mirror");
    libdecor_frame_set_title(ctx->wl.window.libdecor_frame, "Wayland Output Mirror");

    // map libdecor frame
    log_debug(ctx, "wayland::window::on_output_initial_sync(): mapping frame\n");
    libdecor_frame_map(ctx->wl.window.libdecor_frame);
    wl_surface_commit(ctx->wl.window.surface);
}
