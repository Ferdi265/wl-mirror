#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "context.h"

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
            log_debug(ctx, "wayland::on_output_geometry(): updating output %s (transform = ", node->name);
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
            resize_viewport(ctx);
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
        log_debug(ctx, "wayland::on_output_scale(): updating output %s (scale = %d, id = %d)\n", node->name, scale, node->output_id);
        node->scale = scale;

        // update buffer scale only if this is the current output
        if (ctx->wl.current_output != NULL && ctx->wl.current_output->output == output) {
            log_debug(ctx, "wayland::on_output_scale(): updating window scale\n");
            ctx->wl.scale = scale;
            wl_surface_set_buffer_scale(ctx->wl.surface, scale);

            // resize egl window to reflect new scale
            if (ctx->egl.initialized) {
                resize_window(ctx);
            }
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
        log_debug(ctx, "wayland::on_xdg_output_logical_position(): updating output %s (position = %d,%d, id = %d)\n", node->name, x, y, node->output_id);

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
        log_debug(ctx, "wayland::on_xdg_output_logical_size(): updating output %s (size = %dx%d, id = %d)\n", node->name, width, height, node->output_id);

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
        log_debug(ctx, "wayland::on_xdg_output_name(): updating output %s (id = %d)\n", name, node->output_id);

        // allocate copy of name since name is owned by libwayland
        free(node->name);
        node->name = strdup(name);
        if (node->name == NULL) {
            log_error("wayland::on_xdg_output_name(): failed to allocate output name\n");
            exit_fail(ctx);
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

    log_debug(ctx, "wayland::on_registry_add(): %s (version = %d, id = %d)\n", interface, version, id);

    // bind proxy object for each protocol we need
    // bind proxy object for each output
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        if (ctx->wl.compositor != NULL) {
            log_error("wayland::on_registry_add(): duplicate compositor\n");
            exit_fail(ctx);
        }

        // bind compositor object
        ctx->wl.compositor = (struct wl_compositor *)wl_registry_bind(
            registry, id, &wl_compositor_interface, 4
        );
        ctx->wl.compositor_id = id;
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        if (ctx->wl.seat != NULL) {
            log_error("wayland::on_registry_add(): duplicate wl_seat\n");
            exit_fail(ctx);
        }

        // bind wl_seat object
        ctx->wl.seat = (struct wl_seat *)wl_registry_bind(
            registry, id, &wl_seat_interface, 1
        );
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        if (ctx->wl.wm_base != NULL) {
            log_error("wayland::on_registry_add(): duplicate wm_base\n");
            exit_fail(ctx);
        }

        // bind wm_base object
        ctx->wl.wm_base = (struct xdg_wm_base *)wl_registry_bind(
            registry, id, &xdg_wm_base_interface, 2
        );
        ctx->wl.wm_base_id = id;
    } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
        if (ctx->wl.output_manager != NULL) {
            log_error("wayland::on_registry_add(): duplicate output_manager\n");
            exit_fail(ctx);
        }

        // bind output_manager object
        ctx->wl.output_manager = (struct zxdg_output_manager_v1 *)wl_registry_bind(
            registry, id, &zxdg_output_manager_v1_interface, 2
        );
        ctx->wl.output_manager_id = id;
    } else if (strcmp(interface, zwlr_export_dmabuf_manager_v1_interface.name) == 0) {
        if (ctx->wl.dmabuf_manager != NULL) {
            log_error("wayland::on_registry_add(): duplicate dmabuf_manager\n");
            exit_fail(ctx);
        }

        // bind dmabuf manager object
        // - for mirror-dmabuf backend
        ctx->wl.dmabuf_manager = (struct zwlr_export_dmabuf_manager_v1 *)wl_registry_bind(
            registry, id, &zwlr_export_dmabuf_manager_v1_interface, 1
        );
        ctx->wl.dmabuf_manager_id = id;
    } else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        if (ctx->wl.screencopy_manager != NULL) {
            log_error("wayland::on_registry_add(): duplicate screencopy_manager\n");
            exit_fail(ctx);
        }

        // bind screencopy manager object
        // - for mirror-screencopy backend
        ctx->wl.screencopy_manager = (struct zwlr_screencopy_manager_v1 *)wl_registry_bind(
            registry, id, &zwlr_screencopy_manager_v1_interface, 3
        );
        ctx->wl.screencopy_manager_id = id;
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        if (ctx->wl.shm != NULL) {
            log_error("wayland::on_registry_add(): duplicate shm\n");
            exit_fail(ctx);
        }

        // bind shm object
        // - for mirror-screencopy backend
        ctx->wl.shm = (struct wl_shm *)wl_registry_bind(
            registry, id, &wl_shm_interface, 1
        );
        ctx->wl.shm_id = id;
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        // allocate output node
        output_list_node_t * node = malloc(sizeof (output_list_node_t));
        if (node == NULL) {
            log_error("wayland::on_registry_add(): failed to allocate output node\n");
            exit_fail(ctx);
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
            log_error("wayland::on_registry_add(): wl_output received before xdg_output_manager\n");
            exit_fail(ctx);
        }

        // create xdg_output object
        node->xdg_output = (struct zxdg_output_v1 *)zxdg_output_manager_v1_get_xdg_output(
            ctx->wl.output_manager, node->output
        );
        if (node->xdg_output == NULL) {
            log_error("wayland::on_registry_add(): failed to create xdg_output\n");
            exit_fail(ctx);
        }

        // add xdg_output event listener
        // - for logical_position event
        // - for logical_size event
        // - for name event
        zxdg_output_v1_add_listener(node->xdg_output, &xdg_output_listener, (void *)node);
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
        log_error("wayland::on_registry_remove(): compositor disappeared\n");
        exit_fail(ctx);
    } else if (id == ctx->wl.seat_id) {
        log_error("wayland::on_registry_remove(): seat disappeared\n");
        exit_fail(ctx);
    } else if (id == ctx->wl.wm_base_id) {
        log_error("wayland::on_registry_remove(): wm_base disappeared\n");
        exit_fail(ctx);
    } else if (id == ctx->wl.output_manager_id) {
        log_error("wayland::on_registry_remove(): output_manager disappeared\n");
        exit_fail(ctx);
    } else if (id == ctx->wl.dmabuf_manager_id) {
        log_error("wayland::on_registry_remove(): dmabuf_manager disappeared\n");
        exit_fail(ctx);
    } else {
        output_list_node_t ** link = &ctx->wl.outputs;
        output_list_node_t * cur = ctx->wl.outputs;
        output_list_node_t * prev = NULL;
        while (cur != NULL) {
            if (id == cur->output_id) {
                log_debug(ctx, "wayland::on_registry_remove(): output %s removed (id = %d)\n", cur->name, id);

                // notify mirror code of removed outputs
                // - triggers exit if the target output disappears
                output_removed(ctx, cur);

                // remove output node from linked list
                *link = cur->next;
                prev = cur;
                cur = cur->next;

                // deallocate output node
                zxdg_output_v1_destroy(prev->xdg_output);
                wl_output_destroy(prev->output);
                free(prev->name);
                free(prev);

                // break when the removed output was found
                break;
            } else {
                link = &cur->next;
                cur = cur->next;
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
        log_error("wayland::on_surface_enter(): entered nonexistent output\n");
        exit_fail(ctx);
    }

    // update current output
    ctx->wl.current_output = node;

    // update scale only if changed
    if (node->scale != ctx->wl.scale) {
        log_debug(ctx, "wayland::on_surface_enter(): updating window scale\n");
        ctx->wl.scale = node->scale;
        wl_surface_set_buffer_scale(ctx->wl.surface, node->scale);

        // resize egl window to reflect new scale
        if (ctx->egl.initialized) {
            resize_window(ctx);
        }
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
    log_debug(ctx, "wayland::on_surface_configure_finished(): window configured\n");
    xdg_surface_ack_configure(ctx->wl.xdg_surface, ctx->wl.last_surface_serial);
    wl_surface_commit(ctx->wl.surface);

    // reset configure sequence state machine
    ctx->wl.xdg_surface_configured = false;
    ctx->wl.xdg_toplevel_configured = false;
    ctx->wl.configured = true;
}

// --- xdg_surface event handlers ---

static void xdg_on_surface_configure(
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
    .configure = xdg_on_surface_configure,
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

    // update size only if changed
    if (ctx->wl.width != (uint32_t)width || ctx->wl.height != (uint32_t)height) {
        log_debug(ctx, "wayland::on_xdg_toplevel_configure(): window resized to %dx%d\n", width, height);
        ctx->wl.width = width;
        ctx->wl.height = height;

        // resize window to reflect new surface size
        if (ctx->egl.initialized) {
            resize_window(ctx);
        }
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

    log_debug(ctx, "wayland::on_xdg_toplevel_close(): close request received\n");
    ctx->wl.closing = true;

    (void)xdg_toplevel;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = on_xdg_toplevel_configure,
    .close = on_xdg_toplevel_close
};

static void on_loop_event(ctx_t * ctx) {
    if (wl_display_dispatch(ctx->wl.display) == -1) {
        ctx->wl.closing = true;
    }
}

static void on_loop_each(ctx_t * ctx) {
    wl_display_flush(ctx->wl.display);
}

// --- init_wl ---

void init_wl(ctx_t * ctx) {
    // initialize context structure
    ctx->wl.display = NULL;
    ctx->wl.registry = NULL;

    ctx->wl.compositor = NULL;
    ctx->wl.compositor_id = 0;
    ctx->wl.seat = NULL;
    ctx->wl.seat_id = 0;
    ctx->wl.wm_base = NULL;
    ctx->wl.wm_base_id = 0;
    ctx->wl.output_manager = NULL;
    ctx->wl.output_manager_id = 0;

    ctx->wl.dmabuf_manager = NULL;
    ctx->wl.dmabuf_manager_id = 0;
    ctx->wl.shm = NULL;
    ctx->wl.shm_id = 0;
    ctx->wl.screencopy_manager = NULL;
    ctx->wl.screencopy_manager_id = 0;

    ctx->wl.outputs = NULL;

    ctx->wl.surface = NULL;
    ctx->wl.xdg_surface = NULL;
    ctx->wl.xdg_toplevel = NULL;

    ctx->wl.current_output = NULL;
    ctx->wl.width = 0;
    ctx->wl.height = 0;
    ctx->wl.scale = 1;

    ctx->wl.event_handler.fd = -1;
    ctx->wl.event_handler.events = EPOLLIN;
    ctx->wl.event_handler.on_event = on_loop_event;
    ctx->wl.event_handler.on_each = on_loop_each;

    ctx->wl.last_surface_serial = 0;
    ctx->wl.xdg_surface_configured = false;
    ctx->wl.xdg_toplevel_configured = false;
    ctx->wl.configured = false;
    ctx->wl.closing = false;
    ctx->wl.initialized = true;

    // connect to display
    ctx->wl.display = wl_display_connect(NULL);
    if (ctx->wl.display == NULL) {
        log_error("wayland::init(): failed to connect to wayland\n");
        exit_fail(ctx);
    }

    // register event loop
    ctx->wl.event_handler.fd = wl_display_get_fd(ctx->wl.display);
    event_add_fd(ctx, &ctx->wl.event_handler);

    // get registry handle
    ctx->wl.registry = wl_display_get_registry(ctx->wl.display);
    if (ctx->wl.registry == NULL) {
        log_error("wayland::init(): failed to get registry handle\n");
        exit_fail(ctx);
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
        log_error("wayland::init(): compositor missing\n");
        exit_fail(ctx);
    } else if (ctx->wl.wm_base == NULL) {
        log_error("wayland::init(): wm_base missing\n");
        exit_fail(ctx);
    } else if (ctx->wl.output_manager == NULL) {
        log_error("wayland::init(): output_manager missing\n");
        exit_fail(ctx);
    }

    // add wm_base event listener
    // - for ping event
    xdg_wm_base_add_listener(ctx->wl.wm_base, &wm_base_listener, (void *)ctx);

    // create surface
    ctx->wl.surface = wl_compositor_create_surface(ctx->wl.compositor);
    if (ctx->wl.surface == NULL) {
        log_error("wayland::init(): failed to create surface\n");
        exit_fail(ctx);
    }

    // add surface event listener
    // - for enter event
    // - for leave event
    wl_surface_add_listener(ctx->wl.surface, &surface_listener, (void *)ctx);

    // create xdg surface
    ctx->wl.xdg_surface = xdg_wm_base_get_xdg_surface(ctx->wl.wm_base, ctx->wl.surface);
    if (ctx->wl.xdg_surface == NULL) {
        log_error("wayland::init(): failed to create xdg_surface\n");
        exit_fail(ctx);
    }

    // add xdg surface event listener
    // - for configure event
    xdg_surface_add_listener(ctx->wl.xdg_surface, &xdg_surface_listener, (void *)ctx);

    // create xdg toplevel
    ctx->wl.xdg_toplevel = xdg_surface_get_toplevel(ctx->wl.xdg_surface);
    if (ctx->wl.xdg_toplevel == NULL) {
        log_error("wayland::init(): failed to create xdg_toplevel\n");
        exit_fail(ctx);
    }

    // add xdg toplevel event listener
    // - for toplevel configure event
    // - for close event
    xdg_toplevel_add_listener(ctx->wl.xdg_toplevel, &xdg_toplevel_listener, (void *)ctx);

    // set xdg toplevel properties
    xdg_toplevel_set_app_id(ctx->wl.xdg_toplevel, "at.yrlf.wl_mirror");
    xdg_toplevel_set_title(ctx->wl.xdg_toplevel, "Wayland Output Mirror");

    // commit surface to trigger configure sequence
    wl_surface_commit(ctx->wl.surface);

    // wait for events
    // - expecing surface configure event
    // - expecting xdg toplevel configure event
    wl_display_roundtrip(ctx->wl.display);

    // check if surface is configured
    // - expecting surface to be configured at this point
    if (!ctx->wl.configured) {
        log_error("wayland::init(): surface not configured\n");
        exit_fail(ctx);
    }
}

// --- set_window_title ---

void set_window_title(ctx_t * ctx, const char * title) {
    xdg_toplevel_set_title(ctx->wl.xdg_toplevel, title);
}

// --- cleanup_wl ---

void cleanup_wl(ctx_t *ctx) {
    if (!ctx->wl.initialized) return;

    log_debug(ctx, "wayland::cleanup(): destroying wayland objects\n");

    // deregister event handler
    event_remove_fd(ctx, &ctx->wl.event_handler);

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

    if (ctx->wl.dmabuf_manager != NULL) zwlr_export_dmabuf_manager_v1_destroy(ctx->wl.dmabuf_manager);
    if (ctx->wl.screencopy_manager != NULL) zwlr_screencopy_manager_v1_destroy(ctx->wl.screencopy_manager);
    if (ctx->wl.shm != NULL) wl_shm_destroy(ctx->wl.shm);
    if (ctx->wl.xdg_toplevel != NULL) xdg_toplevel_destroy(ctx->wl.xdg_toplevel);
    if (ctx->wl.xdg_surface != NULL) xdg_surface_destroy(ctx->wl.xdg_surface);
    if (ctx->wl.surface != NULL) wl_surface_destroy(ctx->wl.surface);
    if (ctx->wl.output_manager != NULL) zxdg_output_manager_v1_destroy(ctx->wl.output_manager);
    if (ctx->wl.wm_base != NULL) xdg_wm_base_destroy(ctx->wl.wm_base);
    if (ctx->wl.seat != NULL) wl_seat_destroy(ctx->wl.seat);
    if (ctx->wl.compositor != NULL) wl_compositor_destroy(ctx->wl.compositor);
    if (ctx->wl.registry != NULL) wl_registry_destroy(ctx->wl.registry);
    if (ctx->wl.display != NULL) wl_display_disconnect(ctx->wl.display);

    ctx->wl.initialized = false;
}

