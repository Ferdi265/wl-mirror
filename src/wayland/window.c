#include <math.h>
#include <wlm/context.h>

// --- private helper functions ---

static bool use_output_scale(ctx_t * ctx) {
    return ctx->wl.protocols.fractional_scale_manager == NULL && wl_surface_get_version(ctx->wl.window.surface) < 6;
}

static bool use_surface_preferred_scale(ctx_t * ctx) {
    return ctx->wl.protocols.fractional_scale_manager == NULL && wl_surface_get_version(ctx->wl.window.surface) >= 6;
}

static bool use_fractional_preferred_scale(ctx_t * ctx) {
    return ctx->wl.protocols.fractional_scale_manager != NULL;
}

static void apply_surface_changes(ctx_t * ctx) {
    if (!ctx->wl.window.changed) return;

    if (ctx->wl.window.changed & WLM_WAYLAND_WINDOW_SIZE_CHANGED) {
        log_debug(ctx, "wayland::window::apply_surface_changes(): new viewport destination size = %dx%d\n",
            ctx->wl.window.width, ctx->wl.window.height
        );
        wp_viewport_set_destination(ctx->wl.window.viewport, ctx->wl.window.width, ctx->wl.window.height);
    }

    if ((ctx->wl.window.changed & WLM_WAYLAND_WINDOW_SIZE_CHANGED) || (ctx->wl.window.changed & WLM_WAYLAND_WINDOW_SCALE_CHANGED)) {
        uint32_t buffer_width = round(ctx->wl.window.width * ctx->wl.window.scale);
        uint32_t buffer_height = round(ctx->wl.window.height * ctx->wl.window.scale);
        if (ctx->wl.window.buffer_width != buffer_width || ctx->wl.window.buffer_height != buffer_height) {
            log_debug(ctx, "wayland::window::apply_surface_changes(): new buffer size = %dx%d\n", buffer_width, buffer_height);

            ctx->wl.window.buffer_width = buffer_width;
            ctx->wl.window.buffer_height = buffer_height;
            ctx->wl.window.changed |= WLM_WAYLAND_WINDOW_BUFFER_SIZE_CHANGED;
        }
    }

    // TODO: react to preferred transform

    // trigger buffer resize and render
    if (ctx->wl.window.incomplete) {
        wlm_event_emit_window_initial_configure(ctx);
    } else {
        wlm_event_emit_window_changed(ctx);
    }

    // TODO: who performs wl_surface_commit()?
    wl_surface_commit(ctx->wl.window.surface);

    ctx->wl.window.changed = WLM_WAYLAND_WINDOW_UNCHANGED;
    ctx->wl.window.incomplete = WLM_WAYLAND_WINDOW_COMPLETE;
}

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
    if (ctx->wl.window.current_output == entry) return;

    log_debug(ctx, "wayland::window::on_surface_enter(): entering output %s\n", entry->name);

    ctx->wl.window.current_output = entry;
    ctx->wl.window.changed |= WLM_WAYLAND_WINDOW_OUTPUT_CHANGED;

    if (use_output_scale(ctx) && ctx->wl.window.scale != entry->scale) {
        log_debug(ctx, "wayland::window::on_surface_enter(): using output scale = %d\n", entry->scale);

        ctx->wl.window.scale = entry->scale;
        ctx->wl.window.changed |= WLM_WAYLAND_WINDOW_SCALE_CHANGED;
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
    if (ctx->wl.window.current_output != entry) return;

    log_debug(ctx, "wayland::window::on_surface_leave(): leaving output %s\n", entry->name);

    // ignore: current output is set with next entered output anyway
}

static void on_surface_preferred_buffer_scale(
    void * data, struct wl_surface * surface,
    int32_t scale
) {
    if (surface == NULL) return;

    ctx_t * ctx = (ctx_t *)data;

    if (use_surface_preferred_scale(ctx) && ctx->wl.window.scale != scale) {
        log_debug(ctx, "wayland::window::on_surface_preferred_buffer_scale(): using preferred integer scale = %d\n", scale);

        ctx->wl.window.scale = scale;
        ctx->wl.window.changed |= WLM_WAYLAND_WINDOW_SCALE_CHANGED;
    }
}

static void on_surface_preferred_buffer_transform(
    void * data, struct wl_surface * surface,
    uint32_t transform_int
) {
    if (surface == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    enum wl_output_transform transform = (enum wl_output_transform)transform_int;


    if (ctx->wl.window.transform != transform) {
        log_debug(ctx, "wayland::window::on_surface_preferred_buffer_transform(): using preferred transform = %s\n",
            WLM_PRINT_OUTPUT_TRANSFORM(transform)
        );

        ctx->wl.window.transform = transform;
        ctx->wl.window.changed |= WLM_WAYLAND_WINDOW_TRANSFORM_CHANGED;
    }
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

    if (use_fractional_preferred_scale(ctx) && ctx->wl.window.scale != scale) {
        log_debug(ctx, "wayland::window::on_fractional_scale_preferred_scale(): using preferred fractional scale = %.3f\n", scale);

        ctx->wl.window.scale = scale;
        ctx->wl.window.changed |= WLM_WAYLAND_WINDOW_SCALE_CHANGED;
    }
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

    // get minimum window size
    int min_width = 0;
    int min_height = 0;
    libdecor_frame_get_min_content_size(frame, &min_width, &min_height);

    // set minimum window size
    int new_min_width = min_width;
    int new_min_height = min_height;
    if (new_min_width < DEFAULT_WIDTH) new_min_width = DEFAULT_WIDTH;
    if (new_min_height < DEFAULT_HEIGHT) new_min_height = DEFAULT_HEIGHT;
    if (new_min_width != min_width || new_min_height != min_height) {
        log_debug(ctx, "wayland::window::on_libdecor_frame_configure(): setting minimum size = %dx%d\n", new_min_width, new_min_height);
        libdecor_frame_set_min_content_size(frame, new_min_width, new_min_height);
    }

    // get window size
    int width = 0;
    int height = 0;
    if (!libdecor_configuration_get_content_size(configuration, frame, &width, &height)) {
        if (ctx->wl.window.width == 0 || ctx->wl.window.height == 0) {
            log_debug(ctx, "wayland::window::on_libdecor_frame_configure(): falling back to default size\n");
            width = new_min_width;
            height = new_min_height;
        } else {
            log_debug(ctx, "wayland::window::on_libdecor_frame_configure(): falling back to previous size\n");
            width = ctx->wl.window.width;
            height = ctx->wl.window.height;
        }
    }

    // update window size
    if (ctx->wl.window.width != (uint32_t)width || ctx->wl.window.height != (uint32_t)height) {
        ctx->wl.window.width = width;
        ctx->wl.window.height = height;
        ctx->wl.window.changed |= WLM_WAYLAND_WINDOW_SIZE_CHANGED;
    }

    // get fullscreen state
    bool is_fullscreen = false;
    enum libdecor_window_state window_state;
    if (libdecor_configuration_get_window_state(configuration, &window_state)) {
        if (window_state & LIBDECOR_WINDOW_STATE_FULLSCREEN) {
            is_fullscreen = true;
        }
    }

    // TODO: update actual fullscreen state

    // commit window configuration
    struct libdecor_state * state = libdecor_state_new(width, height);
    libdecor_frame_commit(frame, state, configuration);
    libdecor_state_free(state);
    (void)is_fullscreen;

    // check if things changed, emit events
    apply_surface_changes(ctx);
}

static void on_libdecor_frame_commit(
    struct libdecor_frame * frame, void * data
) {
    // ignore: don't need to know when frame committed
    (void)frame;
    (void)data;
}

static void on_libdecor_frame_close(
    struct libdecor_frame * frame, void * data
) {
    if (frame == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    log_debug(ctx, "wayland::window::on_libdecor_frame_close(): close requested\n");

    wlm_wayland_core_request_close(ctx);
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
    // TODO: verify initial sync complete?
    //       if not, trigger this "soon"?
    // TODO: set fullscreen here if already requested

    // set libdecor app properties
    libdecor_frame_set_app_id(ctx->wl.window.libdecor_frame, "at.yrlf.wl_mirror");
    libdecor_frame_set_title(ctx->wl.window.libdecor_frame, "Wayland Output Mirror");

    // map libdecor frame
    log_debug(ctx, "wayland::window::on_output_initial_sync(): mapping frame\n");
    libdecor_frame_map(ctx->wl.window.libdecor_frame);

    // check if things changed, emit events
    apply_surface_changes(ctx);
}

void wlm_wayland_window_on_output_changed(ctx_t * ctx, wlm_wayland_output_entry_t * entry) {
    if (ctx->wl.window.current_output != entry) return;

    if (use_output_scale(ctx) && ctx->wl.window.scale != entry->scale) {
        log_debug(ctx, "wayland::window::on_output_changed(): using new output scale = %d\n", entry->scale);

        ctx->wl.window.scale = entry->scale;
        ctx->wl.window.changed |= WLM_WAYLAND_WINDOW_SCALE_CHANGED;
    }
}

void wlm_wayland_window_on_output_removed(ctx_t * ctx, wlm_wayland_output_entry_t * entry) {
    if (ctx->wl.window.current_output != entry) return;

    // set current output to null because it would dangle otherwise
    ctx->wl.window.current_output = NULL;
}

void wlm_wayland_window_on_before_poll(ctx_t * ctx) {
    if (ctx->wl.window.incomplete) return;

    // check if things changed, emit events
    apply_surface_changes(ctx);
}

// --- initialization and cleanup ---

void wlm_wayland_window_zero(ctx_t * ctx) {
    ctx->wl.window.surface = NULL;
    ctx->wl.window.viewport = NULL;
    ctx->wl.window.fractional_scale = NULL;
    ctx->wl.window.libdecor_frame = NULL;

    ctx->wl.window.current_output = NULL;
    ctx->wl.window.transform = WL_OUTPUT_TRANSFORM_NORMAL;
    ctx->wl.window.width = 0;
    ctx->wl.window.height = 0;
    ctx->wl.window.buffer_width = 0;
    ctx->wl.window.buffer_height = 0;
    ctx->wl.window.scale = 1.0;

    ctx->wl.window.changed = WLM_WAYLAND_WINDOW_UNCHANGED;
    ctx->wl.window.incomplete = WLM_WAYLAND_WINDOW_INCOMPLETE;
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
