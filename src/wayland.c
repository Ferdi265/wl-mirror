#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlm/context.h>

// --- wm_base event handlers ---

static void on_wm_base_ping(
    void * data, struct xdg_wm_base * xdg_wm_base, uint32_t serial
) {
    xdg_wm_base_pong(xdg_wm_base, serial);

    (void)data;
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = on_wm_base_ping
};

// --- surface event handlers ---

static void on_surface_enter(
    void * data, struct wl_surface * surface, struct wl_output * output
) {
    ctx_t * ctx = (ctx_t *)data;

    // find output list node for the entered output
    wlm_wayland_output_entry_t * node = NULL;
    wlm_wayland_output_entry_t * cur = ctx->wl.outputs;
    while (cur != NULL) {
        if (cur->output == output) {
            node = cur;
            break;
        }

        cur = cur->next;
    }

    // verify an output was found
    if (node == NULL) {
        // when multiple registries exist and different parts of the application
        // bind separately to the same wl_output, on_surface_enter will be called
        // multiple times.
        //
        // only one of those calls will give us an output in our list of outputs.
        // we should ignore all other calls, instead of bailing completely
        wlm_log_debug(ctx, "wayland::on_surface_enter(): entered output not in output list, possibly bound from another registry?\n");
        return;
    }

    // update current output
    bool first_output = ctx->wl.current_output == NULL;
    ctx->wl.current_output = node;

    wlm_log_debug(ctx, "wayland::on_surface_enter(): updating window scale\n");
    wlm_wayland_window_update_scale(ctx, node->scale, false);

    // set fullscreen on current output if requested by initial options
    if (first_output && ctx->opt.fullscreen && ctx->opt.fullscreen_output == NULL) {
        wlm_log_debug(ctx, "wayland::on_surface_enter(): fullscreening on current output\n");
        wlm_wayland_window_set_fullscreen(ctx);
    }

    (void)surface;
}

static void on_surface_leave(
    void * data, struct wl_surface * surface, struct wl_output * output
) {
    (void)data;
    (void)surface;
    (void)output;
}

static const struct wl_surface_listener surface_listener = {
    .enter = on_surface_enter,
    .leave = on_surface_leave
};

// --- configure callbacks ---

static void on_surface_configure_finished(ctx_t * ctx) {
    // acknowledge configure and commit surface to finish configure sequence
    wlm_log_debug(ctx, "wayland::on_surface_configure_finished(): window configured\n");
#ifdef WITH_LIBDECOR

#else
    xdg_surface_ack_configure(ctx->wl.xdg_surface, ctx->wl.last_surface_serial);
#endif

    // draw frame to attach and commit buffer
    // reduces number of empty commits
    // required if libdecor is used
    // contains a surface commit, no second commit necessary
    wlm_egl_draw_frame(ctx);

    // reset configure sequence state machine
#ifndef WITH_LIBDECOR
    ctx->wl.xdg_surface_configured = false;
    ctx->wl.xdg_toplevel_configured = false;
#endif
    ctx->wl.configured = true;
}

#ifdef WITH_LIBDECOR
// --- libdecor_frame event handlers ---

static void on_libdecor_frame_configure(
    struct libdecor_frame * frame,
    struct libdecor_configuration * configuration, void * data
) {
    ctx_t * ctx = (ctx_t *)data;

    int width = 0;
    int height = 0;
    if (!libdecor_configuration_get_content_size(configuration, frame, &width, &height)) {
        if (ctx->wl.width == 0 || ctx->wl.height == 0) {
            wlm_log_debug(ctx, "wayland::on_libdecor_frame_configure(): falling back to default width\n");
            width = 100;
            height = 100;
        } else {
            wlm_log_debug(ctx, "wayland::on_libdecor_frame_configure(): falling back to previous width\n");
            width = ctx->wl.width;
            height = ctx->wl.height;
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

    // reset fullscreen option if the window is not fullscreen
    // but only if have already had the chance to fullscreen on the current output
    if (is_fullscreen != ctx->opt.fullscreen && ctx->wl.current_output != NULL) {
        ctx->opt.fullscreen = is_fullscreen;
    }

    struct libdecor_state * state = libdecor_state_new(width, height);
    libdecor_frame_commit(frame, state, configuration);
    libdecor_state_free(state);

    // update size only if changed
    if (ctx->wl.width != (uint32_t)width || ctx->wl.height != (uint32_t)height) {
        wlm_log_debug(ctx, "wayland::on_libdecor_frame_configure(): window resized to %dx%d\n", width, height);
        ctx->wl.width = width;
        ctx->wl.height = height;

        // resize viewport
        wp_viewport_set_destination(ctx->wl.viewport, width, height);

        // resize window to reflect new surface size
        wlm_egl_resize_window(ctx);
    }

    // update configure sequence state machine
    on_surface_configure_finished(ctx);
}

static void on_libdecor_frame_commit(
    struct libdecor_frame * frame, void * data
) {
    ctx_t * ctx = (ctx_t *)data;

    (void)frame;
    (void)ctx;
}

static void on_libdecor_frame_close(
    struct libdecor_frame * frame, void * data
) {
    ctx_t * ctx = (ctx_t *)data;

    wlm_log_debug(ctx, "wayland::on_libdecor_frame_close(): close request received\n");
    wlm_wayland_core_request_close(ctx);

    (void)frame;
}

static struct libdecor_frame_interface libdecor_frame_listener = {
    .configure = on_libdecor_frame_configure,
    .commit = on_libdecor_frame_commit,
    .close = on_libdecor_frame_close,
};

#else
// --- xdg_surface event handlers ---

static void on_xdg_surface_configure(
    void * data, struct xdg_surface * xdg_surface, uint32_t serial
) {
    ctx_t * ctx = (ctx_t *)data;

    // save serial for configure acknowledgement
    ctx->wl.last_surface_serial = serial;

    // update configure sequence state machine
    ctx->wl.xdg_surface_configured = true;
    if (ctx->wl.xdg_surface_configured && ctx->wl.xdg_toplevel_configured) {
        on_surface_configure_finished(ctx);
    }

    (void)xdg_surface;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = on_xdg_surface_configure,
};

// --- xdg_toplevel event handlers ---

static void on_xdg_toplevel_configure(
    void * data, struct xdg_toplevel * xdg_toplevel,
    int32_t width, int32_t height, struct wl_array * states
) {
    ctx_t * ctx = (ctx_t *)data;

    // set default size of 100x100 if compositor does not have a preference
    if (width == 0) width = 100;
    if (height == 0) height = 100;

    // check fullscreen state
    bool is_fullscreen = false;
    uint32_t * state;
    wl_array_for_each(state, states) {
        if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN) {
            is_fullscreen = true;
        }
    }

    // reset fullscreen option if the window is not fullscreen
    // but only if have already had the chance to fullscreen on the current output
    if (is_fullscreen != ctx->opt.fullscreen && ctx->wl.current_output != NULL) {
        ctx->opt.fullscreen = is_fullscreen;
    }

    // update size only if changed
    if (ctx->wl.width != (uint32_t)width || ctx->wl.height != (uint32_t)height) {
        wlm_log_debug(ctx, "wayland::on_xdg_toplevel_configure(): window resized to %dx%d\n", width, height);
        ctx->wl.width = width;
        ctx->wl.height = height;

        // resize viewport
        wp_viewport_set_destination(ctx->wl.viewport, width, height);

        // resize window to reflect new surface size
        wlm_egl_resize_window(ctx);
    }

    // update configure sequence state machine
    ctx->wl.xdg_toplevel_configured = true;
    if (ctx->wl.xdg_surface_configured && ctx->wl.xdg_toplevel_configured) {
        on_surface_configure_finished(ctx);
    }

    (void)xdg_toplevel;
    (void)states;
}

static void on_xdg_toplevel_close(
    void * data, struct xdg_toplevel * xdg_toplevel
) {
    ctx_t * ctx = (ctx_t *)data;

    wlm_log_debug(ctx, "wayland::on_xdg_toplevel_close(): close request received\n");
    wlm_wayland_core_request_close(ctx);

    (void)xdg_toplevel;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = on_xdg_toplevel_configure,
    .close = on_xdg_toplevel_close
};
#endif

// --- fractional scale event handlers ---

static void on_fractional_scale_preferred_scale(void * data, struct wp_fractional_scale_v1 * fractional_scale, uint32_t scale_times_120) {
    ctx_t * ctx = (ctx_t *)data;
    double scale = scale_times_120 / 120.0;
    wlm_log_debug(ctx, "wayland::on_fractional_scale_preferred_scale(): scale = %.4f\n", scale);

    // fractionally scaled surfaces have buffer scale of 1
    wlm_wayland_window_update_scale(ctx, scale, true);

    (void)fractional_scale;
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
    .preferred_scale = on_fractional_scale_preferred_scale
};

// --- find_output ---

// TODO: move to wlm_wayland_output
static bool match_output(wlm_wayland_output_entry_t * node, const char * name) {
    // check if name is equal
    if (node->name != NULL && strcmp(node->name, name) == 0) {
        // matched output name (e.g. 'eDP-1')
        wlm_log_error("DEBUG: matched output name: %s\n", name);
        return true;
    }

    // check if description is equal
    // this is usually make + model + serial on wlroots, but doesn't have to be
    if (node->description != NULL && strcmp(node->description, name) == 0) {
        // matched output description (e.g. 'BOE 0x0BCA Unknown', 'Virtual X11 output via :1')
        wlm_log_error("DEBUG: matched output description: %s\n", name);
        return true;
    }

    // check for make / model / serial
    // piecewise check with early returns

    // don't allow make / model / serial matching for outputs without such info
    if (node->make == NULL && node->model == NULL) {
        wlm_log_error("DEBUG: can't perform make+model matching\n");
        return false;
    }

    // check make
    const char * name_make = name;
    const char * node_make = node->make ? node->make : "Unknown";
    if (strncmp(node_make, name_make, strlen(node_make)) != 0) {
        wlm_log_error("DEBUG: failed to match node make: '%s' vs '%s'\n", node_make, name_make);
        return false;
    }

    // check space after make
    const char * name_sep = name_make + strlen(node_make);
    if (*name_sep == '\0') {
        // matched output make (e.g. 'BOE', 'Acer', 'Foocorp')
        wlm_log_error("DEBUG: matched output make: %s\n", name);
        return true;
    }
    if (*name_sep != ' ') {
        // partial match, fail here (e.g. 'Acer 123' vs 'Aceryx 123')
        wlm_log_error("DEBUG: failed to match node make: '%s' vs '%s' (extra space)\n", node_make, name_make);
        return false;
    }

    // check model
    const char * name_model = name_sep + 1;
    const char * node_model = node->model ? node->model : "Unknown";
    if (strncmp(node_model, name_model, strlen(node_model)) != 0) {
        wlm_log_error("DEBUG: failed to match node model: '%s' vs '%s'\n", node_model, name_model);
        return false;
    }

    // check space after model
    name_sep = name_model + strlen(node_model);
    if (*name_sep == '\0') {
        // matched output make + model (e.g. 'BOE 0x0BCA', 'Acer XB240H', 'Foocorp Foo1')
        wlm_log_error("DEBUG: matched output make+model: %s\n", name);
        return true;
    }
    if (*name_sep != ' ') {
        // partial match, fail here (e.g. 'Acer 123' vs 'Acer 1234')
        wlm_log_error("DEBUG: failed to match node model: '%s' vs '%s' (extra space)\n", node_model, name_model);
        return false;
    }

    // check "serial" (doesn't exist in Wayland, for compatibility with kanshi, allow 'Unknown')
    const char * name_serial = name_sep + 1;
    const char * node_serial = "Unknown"; // always use 'Unknown'
    if (strcmp(node_serial, name_serial) != 0) {
        wlm_log_error("DEBUG: failed to match node serial: '%s' vs '%s'\n", node_serial, name_serial);
        return false;
    }

    // matched make + model + "serial" (e.g. 'BOE 0x0BCA Unknown', 'Acer XB240H Unknown')
    wlm_log_error("DEBUG: matched output make+model+serial: %s\n", name);
    return true;
}

// TODO: move to wlm_wayland_output
bool wlm_wayland_find_output(ctx_t * ctx, const char * output_name, wlm_wayland_output_entry_t ** output) {
    bool found = false;

    wlm_wayland_output_entry_t * cur = ctx->wl.outputs;
    while (cur != NULL) {
        if (match_output(cur, output_name)) {
            break;
        }

        cur = cur->next;
    }

    if (cur != NULL) {
        found = true;
        *output = cur;
    }

    return found;
}

// --- init_wl ---

void wlm_wayland_init(ctx_t * ctx) {
    // initialize context structure

    ctx->wl.surface = NULL;
    ctx->wl.viewport = NULL;
    ctx->wl.fractional_scale = NULL;
#ifdef WITH_LIBDECOR
    ctx->wl.libdecor_context = NULL;
    ctx->wl.libdecor_frame = NULL;
#else
    ctx->wl.xdg_surface = NULL;
    ctx->wl.xdg_toplevel = NULL;
#endif

    ctx->wl.current_output = NULL;
    ctx->wl.width = 0;
    ctx->wl.height = 0;
    ctx->wl.scale = 1.0;

    ctx->wl.last_surface_serial = 0;
#ifndef WITH_LIBDECOR
    ctx->wl.xdg_surface_configured = false;
    ctx->wl.xdg_toplevel_configured = false;
#endif
    ctx->wl.configured = false;
    ctx->wl.initialized = true;

    wlm_wayland_core_zero(ctx);
    wlm_wayland_registry_zero(ctx);
    wlm_wayland_output_zero(ctx);
    wlm_wayland_seat_zero(ctx);

    wlm_wayland_core_init(ctx);
    wlm_wayland_registry_init(ctx);
    wlm_wayland_output_init(ctx);
    wlm_wayland_seat_init(ctx);

    wlm_wayland_shm_init(ctx);
    wlm_wayland_dmabuf_init(ctx);

    // wait for registry events
    // - expecting add global events for all required protocols
    // - expecting add global events for all outputs
    wl_display_roundtrip(wlm_wayland_core_get_display(ctx));

    // TODO: loop until initial sync complete
    // TODO: loop until output list complete
    // TODO: loop until toplevel list complete

    // add wm_base event listener
    // - for ping event
    xdg_wm_base_add_listener(ctx->wl.protocols.xdg_wm_base, &wm_base_listener, (void *)ctx);

    // create surface
    ctx->wl.surface = wl_compositor_create_surface(ctx->wl.protocols.compositor);
    if (ctx->wl.surface == NULL) {
        wlm_log_error("wayland::init(): failed to create surface\n");
        wlm_exit_fail(ctx);
    }

    // add surface event listener
    // - for enter event
    // - for leave event
    wl_surface_add_listener(ctx->wl.surface, &surface_listener, (void *)ctx);

    // create viewport
    ctx->wl.viewport = wp_viewporter_get_viewport(ctx->wl.protocols.viewporter, ctx->wl.surface);
    if (ctx->wl.viewport == NULL) {
        wlm_log_error("wayland::init(): failed to create viewport\n");
        wlm_exit_fail(ctx);
    }

    // create fractional scale if supported
    if (ctx->wl.protocols.fractional_scale_manager != NULL) {
        ctx->wl.fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(ctx->wl.protocols.fractional_scale_manager, ctx->wl.surface);

        wp_fractional_scale_v1_add_listener(ctx->wl.fractional_scale, &fractional_scale_listener, (void *)ctx);
    }
}

// --- configure_window ---

void wlm_wayland_configure_window(struct ctx * ctx) {
    if (!ctx->egl.initialized) {
        wlm_log_error("wayland::configure_window(): egl must be initialized first\n");
        wlm_exit_fail(ctx);
    }

#if WITH_LIBDECOR
    // create libdecor frame
    // - for configure event
    // - for commit event
    // - for close event
    ctx->wl.libdecor_frame = libdecor_decorate(wlm_wayland_core_get_libdecor_context(ctx), ctx->wl.surface, &libdecor_frame_listener, ctx);

    // set libdecor app properties
    libdecor_frame_set_app_id(ctx->wl.libdecor_frame, "at.yrlf.wl_mirror");
    wlm_mirror_update_title(ctx);

    // map libdecor frame
    // commits surface and triggers configure sequence
    libdecor_frame_map(ctx->wl.libdecor_frame);

    // wait for events
    // - expecting libdecor frame configure event
    wl_display_roundtrip(wlm_wayland_core_get_display(ctx));
#else
    // create xdg surface
    ctx->wl.xdg_surface = xdg_wm_base_get_xdg_surface(ctx->wl.protocols.xdg_wm_base, ctx->wl.surface);
    if (ctx->wl.xdg_surface == NULL) {
        wlm_log_error("wayland::init(): failed to create xdg_surface\n");
        wlm_exit_fail(ctx);
    }

    // add xdg surface event listener
    // - for configure event
    xdg_surface_add_listener(ctx->wl.xdg_surface, &xdg_surface_listener, (void *)ctx);

    // create xdg toplevel
    ctx->wl.xdg_toplevel = xdg_surface_get_toplevel(ctx->wl.xdg_surface);
    if (ctx->wl.xdg_toplevel == NULL) {
        wlm_log_error("wayland::init(): failed to create xdg_toplevel\n");
        wlm_exit_fail(ctx);
    }

    // add xdg toplevel event listener
    // - for toplevel configure event
    // - for close event
    xdg_toplevel_add_listener(ctx->wl.xdg_toplevel, &xdg_toplevel_listener, (void *)ctx);

    // set xdg toplevel properties
    xdg_toplevel_set_app_id(ctx->wl.xdg_toplevel, "at.yrlf.wl_mirror");
    wlm_mirror_update_title(ctx);

    // commit surface to trigger configure sequence
    wl_surface_commit(ctx->wl.surface);

    // wait for events
    // - expecting surface configure event
    // - expecting xdg toplevel configure event
    wl_display_roundtrip(wlm_wayland_core_get_display(ctx));
#endif

    // wait until surface is configured
    // - if not, attempt to wait another roundtrip
    // - this can happen with e.g. libdecor, when it doesn't find any plugins
    if (!ctx->wl.configured) {
        wlm_log_warn("wayland::configure_window(): surface not configured, attempting to wait until configure event\n");

        int round_trip_count = 1;
        while (!ctx->wl.configured) {
            round_trip_count++;
            wl_display_roundtrip(ctx->wl.display);
        }

        wlm_log_debug(ctx, "wayland::configure_window(): surface configured after %d round trips\n", round_trip_count);
    }

    // check if surface is configured
    // - expecting surface to be configured at this point
    if (!ctx->wl.configured) {
        wlm_log_error("wayland::configure_window(): surface not configured, exiting\n");
        wlm_exit_fail(ctx);
    }

    // set fullscreen on target output if requested by initial options
    if (ctx->opt.fullscreen && ctx->opt.fullscreen_output != NULL) {
        wlm_log_debug(ctx, "wayland::configure_window(): fullscreening on target output\n");
        wlm_wayland_window_set_fullscreen(ctx);
    }
}

// --- set_window_title ---

void wlm_wayland_window_set_title(ctx_t * ctx, const char * title) {
#ifdef WITH_LIBDECOR
    libdecor_frame_set_title(ctx->wl.libdecor_frame, title);
#else
    xdg_toplevel_set_title(ctx->wl.xdg_toplevel, title);
#endif
}

// --- set_window_fullscreen ---

void wlm_wayland_window_set_fullscreen(ctx_t * ctx) {
    wlm_wayland_output_entry_t * output_node = NULL;
    if (ctx->opt.fullscreen_output == NULL) {
        output_node = ctx->wl.current_output;
    } else if (!wlm_wayland_find_output(ctx, ctx->opt.fullscreen_output, &output_node)) {
        wlm_log_error("wayland::init(): output %s not found\n", ctx->opt.fullscreen_output);
        wlm_exit_fail(ctx);
    }

#ifdef WITH_LIBDECOR
    libdecor_frame_set_fullscreen(ctx->wl.libdecor_frame, output_node->output);
#else
    xdg_toplevel_set_fullscreen(ctx->wl.xdg_toplevel, output_node->output);
#endif
}

void wlm_wayland_window_unset_fullscreen(ctx_t * ctx) {
#ifdef WITH_LIBDECOR
    libdecor_frame_unset_fullscreen(ctx->wl.libdecor_frame);
#else
    xdg_toplevel_unset_fullscreen(ctx->wl.xdg_toplevel);
#endif
}

// --- update_window_scale

void wlm_wayland_window_update_scale(ctx_t * ctx, double scale, bool is_fractional) {
    bool resize = false;

    // don't update scale from other sources if fractional scaling supported
    if (ctx->wl.fractional_scale != NULL && !is_fractional) return;

    if (ctx->wl.scale != scale) {
        wlm_log_debug(ctx, "wayland::update_window_scale(): setting window scale to %.4f\n", scale);
        ctx->wl.scale = scale;
        resize = true;
    }

    // resize egl window to reflect new scale
    if (resize && ctx->egl.initialized) {
        wlm_egl_resize_window(ctx);
    }
}

// --- cleanup_wl ---

void wlm_wayland_cleanup(ctx_t *ctx) {
    if (!ctx->wl.initialized) return;

    wlm_log_debug(ctx, "wayland::cleanup(): destroying wayland objects\n");

    // TODO: remove
    // TODO: ensure name/make/model/description is saved

#ifdef WITH_LIBDECOR
    if (ctx->wl.libdecor_frame != NULL) libdecor_frame_unref(ctx->wl.libdecor_frame);
    if (ctx->wl.libdecor_context != NULL) libdecor_unref(ctx->wl.libdecor_context);
#else
    if (ctx->wl.xdg_toplevel != NULL) xdg_toplevel_destroy(ctx->wl.xdg_toplevel);
    if (ctx->wl.xdg_surface != NULL) xdg_surface_destroy(ctx->wl.xdg_surface);
#endif
    if (ctx->wl.fractional_scale != NULL) wp_fractional_scale_v1_destroy(ctx->wl.fractional_scale);
    if (ctx->wl.viewport != NULL) wp_viewport_destroy(ctx->wl.viewport);
    if (ctx->wl.surface != NULL) wl_surface_destroy(ctx->wl.surface);

    wlm_wayland_shm_cleanup(ctx);
    wlm_wayland_dmabuf_cleanup(ctx);
    wlm_wayland_seat_cleanup(ctx);
    wlm_wayland_output_cleanup(ctx);
    wlm_wayland_registry_cleanup(ctx);
    wlm_wayland_core_cleanup(ctx);

    ctx->wl.initialized = false;
}

