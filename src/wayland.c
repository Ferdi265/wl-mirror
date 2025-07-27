#include "wlm/proto/ext-image-capture-source-v1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlm/context.h>

// --- output event handlers ---

static void on_output_geometry(
    void * data, struct wl_output * output,
    int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
    int32_t subpixel, const char * make, const char * model, int32_t transform
) {
    output_list_node_t * node = (output_list_node_t *)data;
    ctx_t * ctx = node->ctx;

    // update transform only if changed
    if (node->transform != (uint32_t)transform) {
        if (ctx->opt.verbose) {
            wlm_log_debug(ctx, "wayland::on_output_geometry(): updating output %s (transform = ", node->name);
            switch (transform) {
                case WL_OUTPUT_TRANSFORM_NORMAL:
                    fprintf(stderr, "normal");
                    break;
                case WL_OUTPUT_TRANSFORM_90:
                    fprintf(stderr, "90ccw");
                    break;
                case WL_OUTPUT_TRANSFORM_180:
                    fprintf(stderr, "180ccw");
                    break;
                case WL_OUTPUT_TRANSFORM_270:
                    fprintf(stderr, "270ccw");
                    break;
                case WL_OUTPUT_TRANSFORM_FLIPPED:
                    fprintf(stderr, "flipX");
                    break;
                case WL_OUTPUT_TRANSFORM_FLIPPED_90:
                    fprintf(stderr, "flipX-90ccw");
                    break;
                case WL_OUTPUT_TRANSFORM_FLIPPED_180:
                    fprintf(stderr, "flipX-180ccw");
                    break;
                case WL_OUTPUT_TRANSFORM_FLIPPED_270:
                    fprintf(stderr, "flipX-270ccw");
                    break;
                default:
                    fprintf(stderr, "unknown");
                    break;
            }
            fprintf(stderr, ", id = %d)\n", node->output_id);
        }

        node->transform = transform;

        // update egl viewport only if this is the target output
        if (ctx->mirror.initialized && ctx->mirror.current_target->output == output) {
            wlm_egl_resize_viewport(ctx);
        }
    }

    (void)x;
    (void)y;
    (void)physical_width;
    (void)physical_height;
    (void)subpixel;
    (void)make;
    (void)model;
    (void)transform;
}

static void on_output_mode(
    void * data, struct wl_output * output,
    uint32_t flags, int32_t width, int32_t height, int32_t refresh
) {
    (void)data;
    (void)output;
    (void)flags;
    (void)width;
    (void)height;
    (void)refresh;
}

static void on_output_scale(
    void * data, struct wl_output * output,
    int32_t scale
) {
    output_list_node_t * node = (output_list_node_t *)data;
    ctx_t * ctx = node->ctx;

    // update scale only if changed
    if (node->scale != scale) {
        wlm_log_debug(ctx, "wayland::on_output_scale(): updating output %s (scale = %d, id = %d)\n", node->name, scale, node->output_id);
        node->scale = scale;

        // update buffer scale only if this is the current output
        if (ctx->wl.current_output != NULL && ctx->wl.current_output->output == output) {
            wlm_log_debug(ctx, "wayland::on_output_scale(): updating window scale\n");
            wlm_wayland_window_update_scale(ctx, scale, false);
        }
    }
}

static void on_output_done(
    void * data, struct wl_output * output
) {
    (void)data;
    (void)output;
}

static const struct wl_output_listener output_listener = {
    .geometry = on_output_geometry,
    .mode = on_output_mode,
    .scale = on_output_scale,
    .done = on_output_done
};

// --- xdg_output event handlers ---

static void on_xdg_output_description(
    void * data, struct zxdg_output_v1 * xdg_output,
    const char * description
) {
    (void)data;
    (void)xdg_output;
    (void)description;
}

static void on_xdg_output_logical_position(
    void * data, struct zxdg_output_v1 * xdg_output,
    int32_t x, int32_t y
) {
    output_list_node_t * node = (output_list_node_t *)data;
    ctx_t * ctx = node->ctx;

    // update position only if changed
    if (node->x != x || node->y != y) {
        wlm_log_debug(ctx, "wayland::on_xdg_output_logical_position(): updating output %s (position = %d,%d, id = %d)\n", node->name, x, y, node->output_id);

        node->x = x;
        node->y = y;
    }

    (void)xdg_output;
}

static void on_xdg_output_logical_size(
    void * data, struct zxdg_output_v1 * xdg_output,
    int32_t width, int32_t height
) {
    output_list_node_t * node = (output_list_node_t *)data;
    ctx_t * ctx = node->ctx;

    // update size only if changed
    if (node->width != width || node->height != height) {
        wlm_log_debug(ctx, "wayland::on_xdg_output_logical_size(): updating output %s (size = %dx%d, id = %d)\n", node->name, width, height, node->output_id);

        node->width = width;
        node->height = height;
    }

    (void)xdg_output;
}

static void on_xdg_output_name(
    void * data, struct zxdg_output_v1 * xdg_output,
    const char * name
) {
    output_list_node_t * node = (output_list_node_t *)data;
    ctx_t * ctx = node->ctx;

    // update name only if changed
    if (node->name == NULL || strcmp(node->name, name) != 0) {
        wlm_log_debug(ctx, "wayland::on_xdg_output_name(): updating output %s (id = %d)\n", name, node->output_id);

        // allocate copy of name since name is owned by libwayland
        free(node->name);
        node->name = strdup(name);
        if (node->name == NULL) {
            wlm_log_error("wayland::on_xdg_output_name(): failed to allocate output name\n");
            wlm_exit_fail(ctx);
        }
    }

    (void)xdg_output;
}

static void on_xdg_output_done(
    void * data, struct zxdg_output_v1 * xdg_output
) {
    (void)data;
    (void)xdg_output;
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
    .description = on_xdg_output_description,
    .logical_position = on_xdg_output_logical_position,
    .logical_size = on_xdg_output_logical_size,
    .name = on_xdg_output_name,
    .done = on_xdg_output_done
};

// --- registry event handlers ---

static void on_registry_add(
    void * data, struct wl_registry * registry,
    uint32_t id, const char * interface, uint32_t version
) {
    ctx_t * ctx = (ctx_t *)data;

    wlm_log_debug(ctx, "wayland::on_registry_add(): %s (version = %d, id = %d)\n", interface, version, id);

    // bind proxy object for each protocol we need
    // bind proxy object for each output
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        if (ctx->wl.compositor != NULL) {
            wlm_log_error("wayland::on_registry_add(): duplicate compositor\n");
            wlm_exit_fail(ctx);
        }

        // bind compositor object
        ctx->wl.compositor = (struct wl_compositor *)wl_registry_bind(
            registry, id, &wl_compositor_interface, 4
        );
        ctx->wl.compositor_id = id;
    } else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
        if (ctx->wl.viewporter != NULL) {
            wlm_log_error("wayland::on_registry_add(): duplicate wp_viewporter\n");
            wlm_exit_fail(ctx);
        }

        // bind wp_viewporter object
        ctx->wl.viewporter = (struct wp_viewporter *)wl_registry_bind(
            registry, id, &wp_viewporter_interface, 1
        );
    } else if (strcmp(interface, wp_fractional_scale_manager_v1_interface.name) == 0) {
        if (ctx->wl.fractional_scale_manager != NULL) {
            wlm_log_error("wayland::on_registry_add(): duplicate wp_fractional_scale_manager\n");
            wlm_exit_fail(ctx);
        }

        // bind wp_fractional_scale_manager_v1 object
        ctx->wl.fractional_scale_manager = (struct wp_fractional_scale_manager_v1 *)wl_registry_bind(
            registry, id, &wp_fractional_scale_manager_v1_interface, 1
        );
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        if (ctx->wl.wm_base != NULL) {
            wlm_log_error("wayland::on_registry_add(): duplicate wm_base\n");
            wlm_exit_fail(ctx);
        }

        // bind wm_base object
        ctx->wl.wm_base = (struct xdg_wm_base *)wl_registry_bind(
            registry, id, &xdg_wm_base_interface, 2
        );
        ctx->wl.wm_base_id = id;
    } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
        if (ctx->wl.output_manager != NULL) {
            wlm_log_error("wayland::on_registry_add(): duplicate output_manager\n");
            wlm_exit_fail(ctx);
        }

        // bind output_manager object
        ctx->wl.output_manager = (struct zxdg_output_manager_v1 *)wl_registry_bind(
            registry, id, &zxdg_output_manager_v1_interface, 2
        );
        ctx->wl.output_manager_id = id;
    } else if (strcmp(interface, zwlr_export_dmabuf_manager_v1_interface.name) == 0) {
        if (ctx->wl.dmabuf_manager != NULL) {
            wlm_log_error("wayland::on_registry_add(): duplicate dmabuf_manager\n");
            wlm_exit_fail(ctx);
        }

        // bind dmabuf manager object
        // - for mirror-export-dmabuf backend
        ctx->wl.dmabuf_manager = (struct zwlr_export_dmabuf_manager_v1 *)wl_registry_bind(
            registry, id, &zwlr_export_dmabuf_manager_v1_interface, 1
        );
        ctx->wl.dmabuf_manager_id = id;
    } else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        if (ctx->wl.screencopy_manager != NULL) {
            wlm_log_error("wayland::on_registry_add(): duplicate screencopy_manager\n");
            wlm_exit_fail(ctx);
        }

        // bind screencopy manager object
        // - for mirror-screencopy backend
        ctx->wl.screencopy_manager = (struct zwlr_screencopy_manager_v1 *)wl_registry_bind(
            registry, id, &zwlr_screencopy_manager_v1_interface, 3
        );
        ctx->wl.screencopy_manager_id = id;
    } else if (strcmp(interface, ext_image_copy_capture_manager_v1_interface.name) == 0) {
        if (ctx->wl.copy_capture_manager != NULL) {
            wlm_log_error("wayland::on_registry_add(): duplicate screencopy_manager\n");
            wlm_exit_fail(ctx);
        }

        // bind image copy capture manager object
        // - for extcopy backend
        ctx->wl.copy_capture_manager = (struct ext_image_copy_capture_manager_v1 *)wl_registry_bind(
            registry, id, &ext_image_copy_capture_manager_v1_interface, 1
        );
        ctx->wl.copy_capture_manager_id = id;
    } else if (strcmp(interface, ext_output_image_capture_source_manager_v1_interface.name) == 0) {
        if (ctx->wl.output_capture_source_manager != NULL) {
            wlm_log_error("wayland::on_registry_add(): duplicate screencopy_manager\n");
            wlm_exit_fail(ctx);
        }

        // bind output capture source manager object
        // - for extcopy backend
        ctx->wl.output_capture_source_manager = (struct ext_output_image_capture_source_manager_v1 *)wl_registry_bind(
            registry, id, &ext_output_image_capture_source_manager_v1_interface, 1
        );
        ctx->wl.output_capture_source_manager_id = id;
    } else if (strcmp(interface, ext_foreign_toplevel_image_capture_source_manager_v1_interface.name) == 0) {
        if (ctx->wl.toplevel_capture_source_manager != NULL) {
            wlm_log_error("wayland::on_registry_add(): duplicate screencopy_manager\n");
            wlm_exit_fail(ctx);
        }

        // bind toplevel capture source manager object
        // - for extcopy backend
        ctx->wl.toplevel_capture_source_manager = (struct ext_foreign_toplevel_image_capture_source_manager_v1 *)wl_registry_bind(
            registry, id, &ext_foreign_toplevel_image_capture_source_manager_v1_interface, 1
        );
        ctx->wl.toplevel_capture_source_manager_id = id;
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        if (ctx->wl.shm != NULL) {
            wlm_log_error("wayland::on_registry_add(): duplicate shm\n");
            wlm_exit_fail(ctx);
        }

        // bind shm object
        // - for mirror-screencopy backend
        ctx->wl.shm = (struct wl_shm *)wl_registry_bind(
            registry, id, &wl_shm_interface, 1
        );
        ctx->wl.shm_id = id;
    } else if (strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0) {
        if (ctx->wl.linux_dmabuf != NULL) {
            wlm_log_error("wayland::on_registry_add(): duplicate linux_dmabuf\n");
            wlm_exit_fail(ctx);
        }

        // bind linux_dmabuf object
        // - for mirror-screencopy backend
        ctx->wl.linux_dmabuf = (struct zwp_linux_dmabuf_v1 *)wl_registry_bind(
            registry, id, &zwp_linux_dmabuf_v1_interface, 4
        );
        ctx->wl.linux_dmabuf_id = id;
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        // allocate output node
        output_list_node_t * node = malloc(sizeof (output_list_node_t));
        if (node == NULL) {
            wlm_log_error("wayland::on_registry_add(): failed to allocate output node\n");
            wlm_exit_fail(ctx);
        }

        // initialize output node
        node->ctx = ctx;
        node->name = NULL;
        node->xdg_output = NULL;
        node->x = 0;
        node->y = 0;
        node->width = 0;
        node->height = 0;
        node->scale = 1;
        node->transform = 0;

        // prepend output node to output list
        node->next = ctx->wl.outputs;
        ctx->wl.outputs = node;

        // bind wl_output
        node->output = (struct wl_output *)wl_registry_bind(
            registry, id, &wl_output_interface, 3
        );
        node->output_id = id;

        // add output event listener
        // - for geometry event
        // - for scale event
        wl_output_add_listener(node->output, &output_listener, (void *)node);

        // check for xdg_output_manager
        // - sway always sends outputs after protocol extensions
        // - for simplicity, only this event order is supported
        if (ctx->wl.output_manager == NULL) {
            wlm_log_error("wayland::on_registry_add(): wl_output received before xdg_output_manager\n");
            wlm_exit_fail(ctx);
        }

        // create xdg_output object
        node->xdg_output = (struct zxdg_output_v1 *)zxdg_output_manager_v1_get_xdg_output(
            ctx->wl.output_manager, node->output
        );
        if (node->xdg_output == NULL) {
            wlm_log_error("wayland::on_registry_add(): failed to create xdg_output\n");
            wlm_exit_fail(ctx);
        }

        // add xdg_output event listener
        // - for logical_position event
        // - for logical_size event
        // - for name event
        zxdg_output_v1_add_listener(node->xdg_output, &xdg_output_listener, (void *)node);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        // allocate seat node
        seat_list_node_t * node = malloc(sizeof (seat_list_node_t));
        if (node == NULL) {
            wlm_log_error("wayland::on_registry_add(): failed to allocate seat node\n");
            wlm_exit_fail(ctx);
        }

        // initialize seat node
        node->ctx = ctx;

        // prepend seat node to seat list
        node->next = ctx->wl.seats;
        ctx->wl.seats = node;

        // bind wl_seat
        node->seat = (struct wl_seat *)wl_registry_bind(
            registry, id, &wl_seat_interface, 1
        );
        node->seat_id = id;
    }

    (void)version;
}

static void on_registry_remove(
    void * data, struct wl_registry * registry,
    uint32_t id
) {
    ctx_t * ctx = (ctx_t *)data;

    // ensure protocols we need aren't removed
    // remove removed outputs from the output list
    if (id == ctx->wl.compositor_id) {
        wlm_log_error("wayland::on_registry_remove(): compositor disappeared\n");
        wlm_exit_fail(ctx);
    } else if (id == ctx->wl.viewporter_id) {
        wlm_log_error("wayland::on_registry_remove(): viewporter disappeared\n");
        wlm_exit_fail(ctx);
    } else if (id == ctx->wl.fractional_scale_manager_id) {
        wlm_log_error("wayland::on_registry_remove(): fractional_scale_manager disappeared\n");
        wlm_exit_fail(ctx);
    } else if (id == ctx->wl.wm_base_id) {
        wlm_log_error("wayland::on_registry_remove(): wm_base disappeared\n");
        wlm_exit_fail(ctx);
    } else if (id == ctx->wl.output_manager_id) {
        wlm_log_error("wayland::on_registry_remove(): output_manager disappeared\n");
        wlm_exit_fail(ctx);
    } else if (id == ctx->wl.shm_id) {
        wlm_log_error("wayland::on_registry_remove(): shm disappeared\n");
        wlm_exit_fail(ctx);
    } else if (id == ctx->wl.linux_dmabuf_id) {
        wlm_log_error("wayland::on_registry_remove(): linux_dmabuf disappeared\n");
        wlm_exit_fail(ctx);
    } else if (id == ctx->wl.dmabuf_manager_id) {
        wlm_log_error("wayland::on_registry_remove(): dmabuf_manager disappeared\n");
        wlm_exit_fail(ctx);
    } else if (id == ctx->wl.copy_capture_manager_id) {
        wlm_log_error("wayland::on_registry_remove(): copy_capture_manager disappeared\n");
        wlm_exit_fail(ctx);
    } else if (id == ctx->wl.output_capture_source_manager_id) {
        wlm_log_error("wayland::on_registry_remove(): output_capture_source_manager disappeared\n");
        wlm_exit_fail(ctx);
    } else if (id == ctx->wl.toplevel_capture_source_manager_id) {
        wlm_log_error("wayland::on_registry_remove(): toplevel_capture_source_manager disappeared\n");
        wlm_exit_fail(ctx);
    } else {
        {
            output_list_node_t ** link = &ctx->wl.outputs;
            output_list_node_t * cur = ctx->wl.outputs;
            output_list_node_t * prev = NULL;
            while (cur != NULL) {
                if (id == cur->output_id) {
                    wlm_log_debug(ctx, "wayland::on_registry_remove(): output %s removed (id = %d)\n", cur->name, id);

                    // notify mirror code of removed outputs
                    // - triggers exit if the target output disappears
                    wlm_mirror_output_removed(ctx, cur);

                    // remove output node from linked list
                    *link = cur->next;
                    prev = cur;
                    cur = cur->next;

                    // deallocate output node
                    zxdg_output_v1_destroy(prev->xdg_output);
                    wl_output_destroy(prev->output);
                    free(prev->name);
                    free(prev);

                    // return because the removed object was found
                    return;
                } else {
                    link = &cur->next;
                    cur = cur->next;
                }
            }
        }

        // output not found
        // id must have been a seat
        {
            seat_list_node_t ** link = &ctx->wl.seats;
            seat_list_node_t * cur = ctx->wl.seats;
            seat_list_node_t * prev = NULL;
            while (cur != NULL) {
                if (id == cur->seat_id) {
                    wlm_log_debug(ctx, "wayland::on_registry_remove(): seat removed (id = %d)\n", id);

                    // remove seat node from linked list
                    *link = cur->next;
                    prev = cur;
                    cur = cur->next;

                    // deallocate seat node
                    wl_seat_destroy(prev->seat);
                    free(prev);

                    // return because the removed object was found
                    return;
                } else {
                    link = &cur->next;
                    cur = cur->next;
                }
            }
        }
    }

    (void)registry;
}

static const struct wl_registry_listener registry_listener = {
    .global = on_registry_add,
    .global_remove = on_registry_remove
};

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
    output_list_node_t * node = NULL;
    output_list_node_t * cur = ctx->wl.outputs;
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
// --- libdecor event handlers ---

// HACK: needed because on_libdecor_error does not have a userdata parameter
static ctx_t * libdecor_error_context = NULL;

static void on_libdecor_error(
    struct libdecor * libdecor_context,
    enum libdecor_error error, const char * message
) {
    ctx_t * ctx = libdecor_error_context;

    wlm_log_error("wayland::on_libdecor_error(): error %d, %s\n", error, message);
    wlm_exit_fail(ctx);

    (void)libdecor_context;
}

static struct libdecor_interface libdecor_listener = {
    .error = on_libdecor_error,
};

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
    ctx->wl.closing = true;

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
    ctx->wl.closing = true;

    (void)xdg_toplevel;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = on_xdg_toplevel_configure,
    .close = on_xdg_toplevel_close
};
#endif

// --- wayland event loop handlers ---

static void on_wayland_event(ctx_t * ctx, uint32_t events) {
    (void)events;

    if (wl_display_dispatch(ctx->wl.display) == -1) {
        ctx->wl.closing = true;
    }

#ifdef WITH_LIBDECOR
    if (libdecor_dispatch(ctx->wl.libdecor_context, 0) < 0) {
        ctx->wl.closing = true;
    }
#endif
}

static void on_wayland_each(ctx_t * ctx) {
    wl_display_flush(ctx->wl.display);
}

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

bool wlm_wayland_find_output(ctx_t * ctx, char * output_name, struct wl_output ** output) {
    bool found = false;
    output_list_node_t * cur = ctx->wl.outputs;
    while (cur != NULL) {
        if (cur->name != NULL && strcmp(cur->name, output_name) == 0) {
            output_name = cur->name;
            break;
        }
        cur = cur->next;
    }
    if (cur != NULL) {
        found = true;
        *output = cur->output;
    }
    return found;
}

// --- init_wl ---

void wlm_wayland_init(ctx_t * ctx) {
    // initialize context structure
    ctx->wl.display = NULL;
    ctx->wl.registry = NULL;

    ctx->wl.compositor = NULL;
    ctx->wl.compositor_id = 0;
    ctx->wl.viewporter = NULL;
    ctx->wl.viewporter_id = 0;
    ctx->wl.fractional_scale_manager = NULL;
    ctx->wl.fractional_scale_manager_id = 0;
    ctx->wl.wm_base = NULL;
    ctx->wl.wm_base_id = 0;
    ctx->wl.output_manager = NULL;
    ctx->wl.output_manager_id = 0;

    ctx->wl.shm = NULL;
    ctx->wl.shm_id = 0;
    ctx->wl.linux_dmabuf = NULL;
    ctx->wl.linux_dmabuf_id = 0;

    ctx->wl.dmabuf_manager = NULL;
    ctx->wl.dmabuf_manager_id = 0;
    ctx->wl.screencopy_manager = NULL;
    ctx->wl.screencopy_manager_id = 0;

    ctx->wl.copy_capture_manager = NULL;
    ctx->wl.output_capture_source_manager = NULL;
    ctx->wl.toplevel_capture_source_manager = NULL;
    ctx->wl.copy_capture_manager_id = 0;
    ctx->wl.output_capture_source_manager_id = 0;
    ctx->wl.toplevel_capture_source_manager_id = 0;

    ctx->wl.outputs = NULL;
    ctx->wl.seats = NULL;

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

    ctx->wl.event_handler.next = NULL;
    ctx->wl.event_handler.fd = -1;
    ctx->wl.event_handler.events = EPOLLIN;
    ctx->wl.event_handler.timeout_ms = -1;
    ctx->wl.event_handler.on_event = on_wayland_event;
    ctx->wl.event_handler.on_each = on_wayland_each;

    ctx->wl.last_surface_serial = 0;
#ifndef WITH_LIBDECOR
    ctx->wl.xdg_surface_configured = false;
    ctx->wl.xdg_toplevel_configured = false;
#endif
    ctx->wl.configured = false;
    ctx->wl.closing = false;
    ctx->wl.initialized = true;

    wlm_wayland_shm_init(ctx);
    wlm_wayland_dmabuf_init(ctx);

    // connect to display
    ctx->wl.display = wl_display_connect(NULL);
    if (ctx->wl.display == NULL) {
        wlm_log_error("wayland::init(): failed to connect to wayland\n");
        wlm_exit_fail(ctx);
    }

    // register event loop
    ctx->wl.event_handler.fd = wl_display_get_fd(ctx->wl.display);
    wlm_event_add_fd(ctx, &ctx->wl.event_handler);

    // get registry handle
    ctx->wl.registry = wl_display_get_registry(ctx->wl.display);
    if (ctx->wl.registry == NULL) {
        wlm_log_error("wayland::init(): failed to get registry handle\n");
        wlm_exit_fail(ctx);
    }

    // add registry event listener
    // - for add global event
    // - for remove global event
    wl_registry_add_listener(ctx->wl.registry, &registry_listener, (void *)ctx);

    // wait for registry events
    // - expecting add global events for all required protocols
    // - expecting add global events for all outputs
    wl_display_roundtrip(ctx->wl.display);

    // check for missing required protocols
    if (ctx->wl.compositor == NULL) {
        wlm_log_error("wayland::init(): compositor missing\n");
        wlm_exit_fail(ctx);
    } else if (ctx->wl.viewporter == NULL) {
        wlm_log_error("wayland::init(): viewporter missing\n");
        wlm_exit_fail(ctx);
    } else if (ctx->wl.wm_base == NULL) {
        wlm_log_error("wayland::init(): wm_base missing\n");
        wlm_exit_fail(ctx);
    } else if (ctx->wl.output_manager == NULL) {
        wlm_log_error("wayland::init(): output_manager missing\n");
        wlm_exit_fail(ctx);
    }

    // add wm_base event listener
    // - for ping event
    xdg_wm_base_add_listener(ctx->wl.wm_base, &wm_base_listener, (void *)ctx);

    // create surface
    ctx->wl.surface = wl_compositor_create_surface(ctx->wl.compositor);
    if (ctx->wl.surface == NULL) {
        wlm_log_error("wayland::init(): failed to create surface\n");
        wlm_exit_fail(ctx);
    }

    // add surface event listener
    // - for enter event
    // - for leave event
    wl_surface_add_listener(ctx->wl.surface, &surface_listener, (void *)ctx);

    // create viewport
    ctx->wl.viewport = wp_viewporter_get_viewport(ctx->wl.viewporter, ctx->wl.surface);
    if (ctx->wl.viewport == NULL) {
        wlm_log_error("wayland::init(): failed to create viewport\n");
        wlm_exit_fail(ctx);
    }

    // create fractional scale if supported
    if (ctx->wl.fractional_scale_manager != NULL) {
        ctx->wl.fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(ctx->wl.fractional_scale_manager, ctx->wl.surface);

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
    // create libdecor context
    // - for error event
    libdecor_error_context = ctx;
    ctx->wl.libdecor_context = libdecor_new(ctx->wl.display, &libdecor_listener);

    // create libdecor frame
    // - for configure event
    // - for commit event
    // - for close event
    ctx->wl.libdecor_frame = libdecor_decorate(ctx->wl.libdecor_context, ctx->wl.surface, &libdecor_frame_listener, ctx);

    // set libdecor app properties
    libdecor_frame_set_app_id(ctx->wl.libdecor_frame, "at.yrlf.wl_mirror");
    wlm_mirror_update_title(ctx);

    // map libdecor frame
    // commits surface and triggers configure sequence
    libdecor_frame_map(ctx->wl.libdecor_frame);

    // wait for events
    // - expecting libdecor frame configure event
    wl_display_roundtrip(ctx->wl.display);
#else
    // create xdg surface
    ctx->wl.xdg_surface = xdg_wm_base_get_xdg_surface(ctx->wl.wm_base, ctx->wl.surface);
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
    wl_display_roundtrip(ctx->wl.display);
#endif

    // check if surface is configured
    // - expecting surface to be configured at this point
    if (!ctx->wl.configured) {
        wlm_log_error("wayland::configure_window(): surface not configured\n");
        wlm_exit_fail(ctx);
    }

    // set fullscreen on target output if requested by initial options
    if (ctx->opt.fullscreen && ctx->opt.fullscreen_output != NULL) {
        wlm_log_debug(ctx, "wayland::configure_window(): fullscreening on target output\n");
        wlm_wayland_window_set_fullscreen(ctx);
    }
}

// --- close_window ---

void wlm_wayland_window_close(struct ctx * ctx) {
    ctx->wl.closing = true;
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
    struct wl_output * output = NULL;
    if (ctx->opt.fullscreen_output == NULL) {
        output = ctx->wl.current_output->output;
    } else if (!wlm_wayland_find_output(ctx, ctx->opt.fullscreen_output, &output)) {
        wlm_log_error("wayland::init(): output %s not found\n", ctx->opt.fullscreen_output);
        wlm_exit_fail(ctx);
    }

#ifdef WITH_LIBDECOR
    libdecor_frame_set_fullscreen(ctx->wl.libdecor_frame, output);
#else
    xdg_toplevel_set_fullscreen(ctx->wl.xdg_toplevel, output);
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

    wlm_wayland_shm_cleanup(ctx);
    wlm_wayland_dmabuf_cleanup(ctx);

    // deregister event handler
    wlm_event_remove_fd(ctx, &ctx->wl.event_handler);

    {
        // free every output in output list
        output_list_node_t * cur = ctx->wl.outputs;
        output_list_node_t * prev = NULL;
        while (cur != NULL) {
            prev = cur;
            cur = cur->next;

            // deallocate output node
            zxdg_output_v1_destroy(prev->xdg_output);
            wl_output_destroy(prev->output);
            free(prev->name);
            free(prev);
        }
        ctx->wl.outputs = NULL;
    }

    {
        // free every seat in seat list
        seat_list_node_t * cur = ctx->wl.seats;
        seat_list_node_t * prev = NULL;
        while (cur != NULL) {
            prev = cur;
            cur = cur->next;

            // deallocate seat node
            wl_seat_destroy(prev->seat);
            free(prev);
        }
        ctx->wl.seats = NULL;
    }

    if (ctx->wl.copy_capture_manager != NULL) ext_image_copy_capture_manager_v1_destroy(ctx->wl.copy_capture_manager);
    if (ctx->wl.output_capture_source_manager != NULL) ext_output_image_capture_source_manager_v1_destroy(ctx->wl.output_capture_source_manager);
    if (ctx->wl.toplevel_capture_source_manager != NULL) ext_foreign_toplevel_image_capture_source_manager_v1_destroy(ctx->wl.toplevel_capture_source_manager);
    if (ctx->wl.dmabuf_manager != NULL) zwlr_export_dmabuf_manager_v1_destroy(ctx->wl.dmabuf_manager);
    if (ctx->wl.screencopy_manager != NULL) zwlr_screencopy_manager_v1_destroy(ctx->wl.screencopy_manager);
    if (ctx->wl.linux_dmabuf != NULL) zwp_linux_dmabuf_v1_destroy(ctx->wl.linux_dmabuf);
    if (ctx->wl.shm != NULL) wl_shm_destroy(ctx->wl.shm);
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
    if (ctx->wl.output_manager != NULL) zxdg_output_manager_v1_destroy(ctx->wl.output_manager);
    if (ctx->wl.wm_base != NULL) xdg_wm_base_destroy(ctx->wl.wm_base);
    if (ctx->wl.fractional_scale_manager != NULL) wp_fractional_scale_manager_v1_destroy(ctx->wl.fractional_scale_manager);
    if (ctx->wl.viewporter != NULL) wp_viewporter_destroy(ctx->wl.viewporter);
    if (ctx->wl.compositor != NULL) wl_compositor_destroy(ctx->wl.compositor);
    if (ctx->wl.registry != NULL) wl_registry_destroy(ctx->wl.registry);
    if (ctx->wl.display != NULL) wl_display_disconnect(ctx->wl.display);

    ctx->wl.initialized = false;
}

