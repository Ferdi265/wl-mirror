#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "context.h"

// --- output event handlers ---

static void output_event_geometry(
    void * data, struct wl_output * output,
    int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
    int32_t subpixel, const char * make, const char * model, int32_t transform
) {
    output_list_node_t * node = (output_list_node_t *)data;
    ctx_t * ctx = node->ctx;

    if (ctx->opt.verbose) {
        log_debug(ctx, "output: output %s has transform ", node->name);
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
        }
        fprintf(stderr, "\n");
    }

    if (node->transform != (uint32_t)transform) {
        log_debug(ctx, "output: updating output transform\n");
        node->transform = transform;
        if (ctx->mirror.initialized && ctx->mirror.current_target->output == output) {
            resize_viewport_egl(ctx);
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

static void output_event_mode(
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

static void output_event_scale(
    void * data, struct wl_output * output,
    int32_t scale
) {
    output_list_node_t * node = (output_list_node_t *)data;
    ctx_t * ctx = node->ctx;

    log_debug(ctx, "output: output %s has scale %d\n", node->name, scale);
    int32_t old_scale = ctx->wl.scale;
    node->scale = scale;
    if (ctx->wl.current_output != NULL && ctx->wl.current_output->output == output) {
        log_debug(ctx, "output: updating window scale\n");
        ctx->wl.scale = scale;
        wl_surface_set_buffer_scale(ctx->wl.surface, scale);
        if (ctx->egl.initialized && old_scale != scale) {
            resize_window_egl(ctx);
        }
    }
}

static void output_event_done(
    void * data, struct wl_output * output
) {
    (void)data;
    (void)output;
}

static const struct wl_output_listener output_listener = {
    .geometry = output_event_geometry,
    .mode = output_event_mode,
    .scale = output_event_scale,
    .done = output_event_done
};

// --- xdg_output event handlers ---

static void xdg_output_event_description(
    void * data, struct zxdg_output_v1 * xdg_output,
    const char * description
) {
    (void)data;
    (void)xdg_output;
    (void)description;
}

static void xdg_output_event_logical_position(
    void * data, struct zxdg_output_v1 * xdg_output,
    int32_t x, int32_t y
) {
    output_list_node_t * node = (output_list_node_t *)data;
    ctx_t * ctx = node->ctx;

    node->x = x;
    node->y = y;
    log_debug(ctx, "xdg_output: output %s has logical position %d,%d\n", node->name, x, y);

    (void)xdg_output;
}

static void xdg_output_event_logical_size(
    void * data, struct zxdg_output_v1 * xdg_output,
    int32_t width, int32_t height
) {
    output_list_node_t * node = (output_list_node_t *)data;
    ctx_t * ctx = node->ctx;

    node->width = width;
    node->height = height;
    log_debug(ctx, "xdg_output: output %s has logical size %dx%d\n", node->name, width, height);

    (void)xdg_output;
}

static void xdg_output_event_name(
    void * data, struct zxdg_output_v1 * xdg_output,
    const char * name
) {
    output_list_node_t * node = (output_list_node_t *)data;
    ctx_t * ctx = node->ctx;

    free(node->name);
    node->name = strdup(name);
    if (node->name == NULL) {
        log_error("xdg_output: failed to allocate output name\n");
        exit_fail(ctx);
    }
    log_debug(ctx, "xdg_output: found output with name %s\n", node->name);

    (void)xdg_output;
}

static void xdg_output_event_done(
    void * data, struct zxdg_output_v1 * xdg_output
) {
    (void)data;
    (void)xdg_output;
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
    .description = xdg_output_event_description,
    .logical_position = xdg_output_event_logical_position,
    .logical_size = xdg_output_event_logical_size,
    .name = xdg_output_event_name,
    .done = xdg_output_event_done
};

// --- registry event handlers ---

static void registry_event_add(
    void * data, struct wl_registry * registry,
    uint32_t id, const char * interface, uint32_t version
) {
    ctx_t * ctx = (ctx_t *)data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        if (ctx->wl.compositor != NULL) {
            log_error("registry: duplicate compositor\n");
            exit_fail(ctx);
        }

        log_debug(ctx, "registry: binding compositor\n");
        ctx->wl.compositor = (struct wl_compositor *)wl_registry_bind(
            registry, id, &wl_compositor_interface, 4
        );
        ctx->wl.compositor_id = id;
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        if (ctx->wl.wm_base != NULL) {
            log_error("registry: duplicate wm_base\n");
            exit_fail(ctx);
        }

        log_debug(ctx, "registry: binding wm_base\n");
        ctx->wl.wm_base = (struct xdg_wm_base *)wl_registry_bind(
            registry, id, &xdg_wm_base_interface, 2
        );
        ctx->wl.wm_base_id = id;
    } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
        if (ctx->wl.output_manager != NULL) {
            log_error("registry: duplicate output_manager\n");
            exit_fail(ctx);
        }

        log_debug(ctx, "registry: binding output_manager\n");
        ctx->wl.output_manager = (struct zxdg_output_manager_v1 *)wl_registry_bind(
            registry, id, &zxdg_output_manager_v1_interface, 2
        );
        ctx->wl.output_manager_id = id;

        log_debug(ctx, "registry: creating xdg_outputs for previously detected outputs\n");
        output_list_node_t * cur = ctx->wl.outputs;
        while (cur != NULL) {
            log_debug(ctx, "registry: creating xdg_output\n");
            cur->xdg_output = (struct zxdg_output_v1 *)zxdg_output_manager_v1_get_xdg_output(
                ctx->wl.output_manager, cur->output
            );
            if (cur->xdg_output == NULL) {
                log_error("registry: failed to create xdg_output\n");
                exit_fail(ctx);
            }

            log_debug(ctx, "registry: adding xdg_output name event listener\n");
            zxdg_output_v1_add_listener(cur->xdg_output, &xdg_output_listener, (void *)cur);

            cur = cur->next;
        }
    } else if (strcmp(interface, zwlr_export_dmabuf_manager_v1_interface.name) == 0) {
        if (ctx->wl.dmabuf_manager != NULL) {
            log_error("registry: duplicate dmabuf_manager\n");
            exit_fail(ctx);
        }

        log_debug(ctx, "registry: binding dmabuf_manager\n");
        ctx->wl.dmabuf_manager = (struct zwlr_export_dmabuf_manager_v1 *)wl_registry_bind(
            registry, id, &zwlr_export_dmabuf_manager_v1_interface, 1
        );
        ctx->wl.dmabuf_manager_id = id;
    } else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        if (ctx->wl.screencopy_manager != NULL) {
            log_error("registry: duplicate screencopy_manager\n");
            exit_fail(ctx);
        }

        log_debug(ctx, "registry: binding screencopy_manager\n");
        ctx->wl.screencopy_manager = (struct zwlr_screencopy_manager_v1 *)wl_registry_bind(
            registry, id, &zwlr_screencopy_manager_v1_interface, 3
        );
        ctx->wl.screencopy_manager_id = id;
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        if (ctx->wl.shm != NULL) {
            log_error("registry: duplicate shm\n");
            exit_fail(ctx);
        }

        log_debug(ctx, "registry: binding shm\n");
        ctx->wl.shm = (struct wl_shm *)wl_registry_bind(
            registry, id, &wl_shm_interface, 1
        );
        ctx->wl.shm_id = id;
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        log_debug(ctx, "registry: allocating output node\n");
        output_list_node_t * node = malloc(sizeof (output_list_node_t));
        if (node == NULL) {
            log_error("registry: failed to allocate output node\n");
            exit_fail(ctx);
        }

        node->ctx = ctx;
        node->name = NULL;
        node->xdg_output = NULL;
        node->x = 0;
        node->y = 0;
        node->width = 0;
        node->height = 0;
        node->scale = 1;
        node->transform = 0;

        log_debug(ctx, "registry: linking output node\n");
        node->next = ctx->wl.outputs;
        ctx->wl.outputs = node;

        log_debug(ctx, "registry: binding output\n");
        node->output = (struct wl_output *)wl_registry_bind(
            registry, id, &wl_output_interface, 3
        );
        node->output_id = id;

        log_debug(ctx, "registry: adding output scale event listener\n");
        wl_output_add_listener(node->output, &output_listener, (void *)node);

        if (ctx->wl.output_manager != NULL) {
            log_debug(ctx, "registry: creating xdg_output\n");
            node->xdg_output = (struct zxdg_output_v1 *)zxdg_output_manager_v1_get_xdg_output(
                ctx->wl.output_manager, node->output
            );
            if (node->xdg_output == NULL) {
                log_error("registry: failed to create xdg_output\n");
                exit_fail(ctx);
            }

            log_debug(ctx, "registry: adding xdg_output name event listener\n");
            zxdg_output_v1_add_listener(node->xdg_output, &xdg_output_listener, (void *)node);
        } else {
            log_debug(ctx, "registry: deferring creation of xdg_output\n");
        }
    }

    (void)version;
}

static void registry_event_remove(
    void * data, struct wl_registry * registry,
    uint32_t id
) {
    ctx_t * ctx = (ctx_t *)data;

    if (id == ctx->wl.compositor_id) {
        log_error("registry: compositor disapperared\n");
        exit_fail(ctx);
    } else if (id == ctx->wl.wm_base_id) {
        log_error("registry: wm_base disapperared\n");
        exit_fail(ctx);
    } else if (id == ctx->wl.output_manager_id) {
        log_error("registry: output_manager disapperared\n");
        exit_fail(ctx);
    } else if (id == ctx->wl.dmabuf_manager_id) {
        log_error("registry: dmabuf_manager disapperared\n");
        exit_fail(ctx);
    } else {
        output_list_node_t ** link = &ctx->wl.outputs;
        output_list_node_t * cur = ctx->wl.outputs;
        output_list_node_t * prev = NULL;
        while (cur != NULL) {
            if (id == cur->output_id) {
                log_debug(ctx, "registry: output %s disappeared\n", cur->name);
                output_removed_mirror(ctx, cur);

                log_debug(ctx, "registry: unlinking output node\n");
                *link = cur->next;
                prev = cur;
                cur = cur->next;

                log_debug(ctx, "registry: deallocating output node\n");
                zxdg_output_v1_destroy(prev->xdg_output);
                wl_output_destroy(prev->output);
                free(prev->name);
                free(prev);
            } else {
                link = &cur->next;
                cur = cur->next;
            }
        }
    }

    (void)registry;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_event_add,
    .global_remove = registry_event_remove
};

// --- wm_base event handlers ---

static void wm_base_event_ping(
    void * data, struct xdg_wm_base * xdg_wm_base, uint32_t serial
) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_wm_base_pong(xdg_wm_base, serial);

    (void)ctx;
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = wm_base_event_ping
};

// --- surface event handlers ---

static void surface_event_enter(
    void * data, struct wl_surface * surface, struct wl_output * output
) {
    ctx_t * ctx = (ctx_t *)data;

    log_debug(ctx, "surface: entering new output\n");
    output_list_node_t * found = NULL;
    output_list_node_t * cur = ctx->wl.outputs;
    while (cur != NULL) {
        if (cur->output == output) {
            found = cur;
            break;
        }

        cur = cur->next;
    }

    if (found == NULL) {
        log_error("surface: entered nonexistant output\n");
        exit_fail(ctx);
    }

    int32_t old_scale = ctx->wl.scale;
    ctx->wl.current_output = found;
    ctx->wl.scale = found->scale;
    if (ctx->egl.initialized && old_scale != found->scale) {
        log_debug(ctx, "surface: updating window scale\n");
        wl_surface_set_buffer_scale(ctx->wl.surface, found->scale);
        resize_window_egl(ctx);
    }

    (void)surface;
}

static void surface_event_leave(
    void * data, struct wl_surface * surface, struct wl_output * output
) {
    (void)data;
    (void)surface;
    (void)output;
}

static const struct wl_surface_listener surface_listener = {
    .enter = surface_event_enter,
    .leave = surface_event_leave
};

// --- configure callbacks ---

static void surface_configure_finished(ctx_t * ctx) {
    log_debug(ctx, "surface_configure_finished: acknowledging configure\n");
    xdg_surface_ack_configure(ctx->wl.xdg_surface, ctx->wl.last_surface_serial);

    log_debug(ctx, "surface_configure_finished: committing surface\n");
    wl_surface_commit(ctx->wl.surface);

    ctx->wl.xdg_surface_configured = false;
    ctx->wl.xdg_toplevel_configured = false;
    ctx->wl.configured = true;
}

// --- xdg_surface event handlers ---

static void xdg_surface_event_configure(
    void * data, struct xdg_surface * xdg_surface, uint32_t serial
) {
    ctx_t * ctx = (ctx_t *)data;
    log_debug(ctx, "xdg_surface: configure\n");

    ctx->wl.last_surface_serial = serial;
    ctx->wl.xdg_surface_configured = true;
    if (ctx->wl.xdg_surface_configured && ctx->wl.xdg_toplevel_configured) {
        surface_configure_finished(ctx);
    }

    (void)xdg_surface;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_event_configure,
};

// --- xdg_toplevel event handlers ---

static void xdg_toplevel_event_configure(
    void * data, struct xdg_toplevel * xdg_toplevel,
    int32_t width, int32_t height, struct wl_array * states
) {
    ctx_t * ctx = (ctx_t *)data;
    log_debug(ctx, "xdg_toplevel: configure\n");

    if (width == 0) width = 100;
    if (height == 0) height = 100;

    int32_t old_width = ctx->wl.width;
    int32_t old_height = ctx->wl.height;
    ctx->wl.width = width;
    ctx->wl.height = height;
    if (ctx->egl.initialized && (width != old_width || height != old_height)) {
        log_debug(ctx, "xdg_toplevel: resize\n");
        resize_window_egl(ctx);
    }

    ctx->wl.xdg_toplevel_configured = true;
    if (ctx->wl.xdg_surface_configured && ctx->wl.xdg_toplevel_configured) {
        surface_configure_finished(ctx);
    }

    (void)xdg_toplevel;
    (void)states;
}

static void xdg_toplevel_event_close(
    void * data, struct xdg_toplevel * xdg_toplevel
) {
    ctx_t * ctx = (ctx_t *)data;
    log_debug(ctx, "xdg_surface: closing\n");
    ctx->wl.closing = true;

    (void)xdg_toplevel;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_event_configure,
    .close = xdg_toplevel_event_close
};

// --- init_wl ---

void init_wl(ctx_t * ctx) {
    log_debug(ctx, "init_wl: initializing context structure\n");

    ctx->wl.display = NULL;
    ctx->wl.registry = NULL;

    ctx->wl.compositor = NULL;
    ctx->wl.compositor_id = 0;
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

    ctx->wl.last_surface_serial = 0;
    ctx->wl.xdg_surface_configured = false;
    ctx->wl.xdg_toplevel_configured = false;
    ctx->wl.configured = false;
    ctx->wl.closing = false;
    ctx->wl.initialized = true;

    log_debug(ctx, "init_wl: connecting to wayland display\n");
    ctx->wl.display = wl_display_connect(NULL);
    if (ctx->wl.display == NULL) {
        log_error("init_wl: failed to connect to wayland\n");
        exit_fail(ctx);
    }

    log_debug(ctx, "init_wl: getting registry\n");
    ctx->wl.registry = wl_display_get_registry(ctx->wl.display);

    log_debug(ctx, "init_wl: adding registry event listener\n");
    wl_registry_add_listener(ctx->wl.registry, &registry_listener, (void *)ctx);

    log_debug(ctx, "init_wl: waiting for events\n");
    wl_display_roundtrip(ctx->wl.display);

    log_debug(ctx, "init_wl: checking for missing protocols\n");
    if (ctx->wl.compositor == NULL) {
        log_error("init_wl: compositor missing\n");
        exit_fail(ctx);
    } else if (ctx->wl.wm_base == NULL) {
        log_error("init_wl: wm_base missing\n");
        exit_fail(ctx);
    } else if (ctx->wl.output_manager == NULL) {
        log_error("init_wl: output_manager missing\n");
        exit_fail(ctx);
    }

    log_debug(ctx, "init_wl: adding wm_base ping event listener\n");
    xdg_wm_base_add_listener(ctx->wl.wm_base, &wm_base_listener, (void *)ctx);

    log_debug(ctx, "init_wl: creating surface\n");
    ctx->wl.surface = wl_compositor_create_surface(ctx->wl.compositor);
    if (ctx->wl.surface == NULL) {
        log_error("init_wl: failed to create surface\n");
        exit_fail(ctx);
    }

    log_debug(ctx, "init_wl: adding surface enter event listener\n");
    wl_surface_add_listener(ctx->wl.surface, &surface_listener, (void *)ctx);

    log_debug(ctx, "init_wl: creating xdg_surface\n");
    ctx->wl.xdg_surface = xdg_wm_base_get_xdg_surface(ctx->wl.wm_base, ctx->wl.surface);
    if (ctx->wl.xdg_surface == NULL) {
        log_error("init_wl: failed to create xdg_surface\n");
        exit_fail(ctx);
    }

    log_debug(ctx, "init_wl: adding xdg_surface configure event listener\n");
    xdg_surface_add_listener(ctx->wl.xdg_surface, &xdg_surface_listener, (void *)ctx);

    log_debug(ctx, "creating xdg_toplevel\n");
    ctx->wl.xdg_toplevel = xdg_surface_get_toplevel(ctx->wl.xdg_surface);
    if (ctx->wl.xdg_toplevel == NULL) {
        log_error("init_wl: failed to create xdg_toplevel\n");
        exit_fail(ctx);
    }

    log_debug(ctx, "init_wl: adding xdg_toplevel event listener\n");
    xdg_toplevel_add_listener(ctx->wl.xdg_toplevel, &xdg_toplevel_listener, (void *)ctx);

    log_debug(ctx, "init_wl: setting xdg_toplevel properties\n");
    xdg_toplevel_set_app_id(ctx->wl.xdg_toplevel, "at.yrlf.wl_mirror");
    xdg_toplevel_set_title(ctx->wl.xdg_toplevel, "Wayland Output Mirror");

    log_debug(ctx, "init_wl: committing surface to trigger configure events\n");
    wl_surface_commit(ctx->wl.surface);

    log_debug(ctx, "init_wl: waiting for events\n");
    wl_display_roundtrip(ctx->wl.display);

    log_debug(ctx, "init_wl: checking if surface configured\n");
    if (!ctx->wl.configured) {
        log_error("init_wl: surface not configured\n");
        exit_fail(ctx);
    }
}

// --- cleanup_wl ---

void cleanup_wl(ctx_t *ctx) {
    if (!ctx->wl.initialized) return;

    log_debug(ctx, "cleanup_wl: destroying wayland objects\n");

    output_list_node_t * cur = ctx->wl.outputs;
    output_list_node_t * prev = NULL;
    while (cur != NULL) {
        prev = cur;
        cur = cur->next;

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
    if (ctx->wl.compositor != NULL) wl_compositor_destroy(ctx->wl.compositor);
    if (ctx->wl.registry != NULL) wl_registry_destroy(ctx->wl.registry);
    if (ctx->wl.display != NULL) wl_display_disconnect(ctx->wl.display);

    ctx->wl.initialized = false;
}

