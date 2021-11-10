#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client-protocol.h>
#include "context.h"

// --- registry event handlers ---

static void registry_event_add(
    void * data, struct wl_registry * registry,
    uint32_t id, const char * interface, uint32_t version
) {
    ctx_t * ctx = (ctx_t *)data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        if (ctx->wl->compositor != NULL) {
            printf("[error] registry: duplicate compositor\n");
            exit_fail(ctx);
        }

        printf("[info] registry: binding compositor\n");
        ctx->wl->compositor = (struct wl_compositor *)wl_registry_bind(
            registry, id, &wl_compositor_interface, 4
        );
        ctx->wl->compositor_id = id;
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        if (ctx->wl->wm_base != NULL) {
            printf("[error] registry: duplicate wm_base\n");
            exit_fail(ctx);
        }

        printf("[info] registry: binding wm_base\n");
        ctx->wl->wm_base = (struct xdg_wm_base *)wl_registry_bind(
            registry, id, &xdg_wm_base_interface, 2
        );
        ctx->wl->wm_base_id = id;
    } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
        if (ctx->wl->output_manager != NULL) {
            printf("[error] registry: duplicate output_manager\n");
            exit_fail(ctx);
        }

        printf("[info] registry: binding output_manager\n");
        ctx->wl->output_manager = (struct zxdg_output_manager_v1 *)wl_registry_bind(
            registry, id, &zxdg_output_manager_v1_interface, 2
        );
        ctx->wl->output_manager_id = id;
    } else if (strcmp(interface, zwlr_export_dmabuf_manager_v1_interface.name) == 0) {
        if (ctx->wl->dmabuf_manager != NULL) {
            printf("[error] registry: duplicate dmabuf_manager\n");
            exit_fail(ctx);
        }

        printf("[info] registry: binding dmabuf_manager\n");
        ctx->wl->dmabuf_manager = (struct zwlr_export_dmabuf_manager_v1 *)wl_registry_bind(
            registry, id, &zwlr_export_dmabuf_manager_v1_interface, 1
        );
        ctx->wl->dmabuf_manager_id = id;
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        printf("[info] registry: allocating output node\n");
        output_list_node_t * node = malloc(sizeof (output_list_node_t));
        if (node == NULL) {
            printf("[error] registry: failed to allocate output node\n");
            exit_fail(ctx);
        }

        node->name = NULL;

        printf("[info] registry: binding output\n");
        node->output = (struct wl_output *)wl_registry_bind(
            registry, id, &wl_output_interface, 3
        );
        node->output_id = id;

        printf("[info] registry: linking output node\n");
        node->next = ctx->wl->outputs;
        ctx->wl->outputs = node;
    }
}

static void registry_event_remove(
    void * data, struct wl_registry * registry,
    uint32_t id
) {
    ctx_t * ctx = (ctx_t *)data;

    if (id == ctx->wl->compositor_id) {
        printf("[error] registry: compositor disapperared\n");
        exit_fail(ctx);
    } else if (id == ctx->wl->wm_base_id) {
        printf("[error] registry: wm_base disapperared\n");
        exit_fail(ctx);
    } else if (id == ctx->wl->output_manager_id) {
        printf("[error] registry: output_manager disapperared\n");
        exit_fail(ctx);
    } else if (id == ctx->wl->dmabuf_manager_id) {
        printf("[error] registry: dmabuf_manager disapperared\n");
        exit_fail(ctx);
    } else {
        output_list_node_t ** link = &ctx->wl->outputs;
        output_list_node_t * cur = ctx->wl->outputs;
        output_list_node_t * prev = NULL;
        while (cur != NULL) {
            if (id == cur->output_id) {
                printf("[info] registry: output %s disappeared\n", cur->name);
                output_removed_handler_mirror(ctx, cur);

                printf("[info] registry: unlinking output node\n");
                *link = cur->next;
                prev = cur;
                cur = cur->next;

                printf("[info] registry: deallocating output node\n");
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

// --- configure callbacks ---

static void surface_configure_finished(ctx_t * ctx) {
    printf("[info] surface_configure_finished: acknowledging configure\n");
    xdg_surface_ack_configure(ctx->wl->xdg_surface, ctx->wl->last_surface_serial);

    printf("[info] surface_configure_finished: committing surface\n");
    wl_surface_commit(ctx->wl->surface);

    ctx->wl->xdg_surface_configured = false;
    ctx->wl->xdg_toplevel_configured = false;
    ctx->wl->configured = true;
}

// --- xdg_surface event handlers ---

static void xdg_surface_event_configure(
    void * data, struct xdg_surface * xdg_surface, uint32_t serial
) {
    ctx_t * ctx = (ctx_t *)data;
    printf("[info] xdg_surface: configure\n");

    ctx->wl->last_surface_serial = serial;
    ctx->wl->xdg_surface_configured = true;
    if (ctx->wl->xdg_surface_configured && ctx->wl->xdg_toplevel_configured) {
        surface_configure_finished(ctx);
    }
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
    printf("[info] xdg_toplevel: configure\n");

    if (width == 0) width = 100;
    if (height == 0) height = 100;

    if (ctx->egl != NULL && (width != ctx->wl->width || height != ctx->wl->height)) {
        printf("[info] xdg_toplevel: resize\n");
        configure_resize_handler_egl(ctx, width, height);
        configure_resize_handler_mirror(ctx, width, height);
    }
    ctx->wl->width = width;
    ctx->wl->height = height;

    ctx->wl->xdg_toplevel_configured = true;
    if (ctx->wl->xdg_surface_configured && ctx->wl->xdg_toplevel_configured) {
        surface_configure_finished(ctx);
    }

    (void)states;
}

static void xdg_toplevel_event_close(
    void * data, struct xdg_toplevel * xdg_toplevel
) {
    ctx_t * ctx = (ctx_t *)data;
    printf("[xdg_surface] close\n");

    printf("[info] closing\n");
    ctx->wl->closing = true;

    (void)ctx;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_event_configure,
    .close = xdg_toplevel_event_close
};

// --- init_wl ---

void init_wl(ctx_t * ctx) {
    printf("[info] init_wl: allocating context structure\n");
    ctx->wl = malloc(sizeof (ctx_wl_t));
    if (ctx->wl == NULL) {
        printf("[error] init_wl: failed to allocate context structure\n");
        exit_fail(ctx);
    }

    ctx->wl->display = NULL;
    ctx->wl->registry = NULL;

    ctx->wl->compositor = NULL;
    ctx->wl->compositor_id = 0;
    ctx->wl->wm_base = NULL;
    ctx->wl->wm_base_id = 0;
    ctx->wl->output_manager = NULL;
    ctx->wl->output_manager_id = 0;
    ctx->wl->dmabuf_manager = NULL;
    ctx->wl->dmabuf_manager_id = 0;

    ctx->wl->outputs = NULL;

    ctx->wl->surface = NULL;
    ctx->wl->xdg_surface = NULL;
    ctx->wl->xdg_toplevel = NULL;

    ctx->wl->width = 0;
    ctx->wl->height = 0;

    ctx->wl->last_surface_serial = 0;
    ctx->wl->xdg_surface_configured = false;
    ctx->wl->xdg_toplevel_configured = false;
    ctx->wl->configured = false;
    ctx->wl->closing = false;

    printf("[info] init_wl: connecting to wayland display\n");
    ctx->wl->display = wl_display_connect(NULL);
    if (ctx->wl->display == NULL) {
        printf("[error] init_wl: failed to connect to wayland\n");
        exit_fail(ctx);
    }

    printf("[info] init_wl: getting registry\n");
    ctx->wl->registry = wl_display_get_registry(ctx->wl->display);

    printf("[info] init_wl: adding registry event listener\n");
    wl_registry_add_listener(ctx->wl->registry, &registry_listener, (void *)ctx);

    printf("[info] init_wl: waiting for events\n");
    wl_display_roundtrip(ctx->wl->display);

    printf("[info] init_wl: checking for missing protocols\n");
    if (ctx->wl->compositor == NULL) {
        printf("[error] init_wl: compositor missing\n");
        exit_fail(ctx);
    } else if (ctx->wl->wm_base == NULL) {
        printf("[error] init_wl: wm_base missing\n");
        exit_fail(ctx);
    } else if (ctx->wl->output_manager == NULL) {
        printf("[error] init_wl: output_manager missing\n");
        exit_fail(ctx);
    } else if (ctx->wl->dmabuf_manager == NULL) {
        printf("[error] init_wl: dmabuf_manager missing\n");
        exit_fail(ctx);
    }

    printf("[info] init_wl: adding wm_base ping event listener\n");
    xdg_wm_base_add_listener(ctx->wl->wm_base, &wm_base_listener, (void *)ctx);

    printf("[info] init_wl: creating surface\n");
    ctx->wl->surface = wl_compositor_create_surface(ctx->wl->compositor);
    if (ctx->wl->surface == NULL) {
        printf("[error] init_wl: failed to create surface\n");
        exit_fail(ctx);
    }

    printf("[info] init_wl: creating xdg_surface\n");
    ctx->wl->xdg_surface = xdg_wm_base_get_xdg_surface(ctx->wl->wm_base, ctx->wl->surface);
    if (ctx->wl->xdg_surface == NULL) {
        printf("[error] init_wl: failed to create xdg_surface\n");
        exit_fail(ctx);
    }

    printf("[info] init_wl: adding xdg_surface configure event listener\n");
    xdg_surface_add_listener(ctx->wl->xdg_surface, &xdg_surface_listener, (void *)ctx);

    printf("[info] creating xdg_toplevel\n");
    ctx->wl->xdg_toplevel = xdg_surface_get_toplevel(ctx->wl->xdg_surface);
    if (ctx->wl->xdg_toplevel == NULL) {
        printf("[error] init_wl: failed to create xdg_toplevel\n");
        exit_fail(ctx);
    }

    printf("[info] init_wl: adding xdg_toplevel event listener\n");
    xdg_toplevel_add_listener(ctx->wl->xdg_toplevel, &xdg_toplevel_listener, (void *)ctx);

    printf("[info] init_wl: setting xdg_toplevel properties\n");
    xdg_toplevel_set_app_id(ctx->wl->xdg_toplevel, "at.yrlf.wl_mirror");
    xdg_toplevel_set_title(ctx->wl->xdg_toplevel, "Wayland Output Mirror");

    printf("[info] init_wl: committing surface to trigger configure events\n");
    wl_surface_commit(ctx->wl->surface);

    printf("[info] init_wl: waiting for events\n");
    wl_display_roundtrip(ctx->wl->display);

    printf("[info] init_wl: checking if surface configured\n");
    if (!ctx->wl->configured) {
        printf("[error] init_wl: surface not configured\n");
        exit_fail(ctx);
    }
}

// --- cleanup_wl ---

void cleanup_wl(ctx_t *ctx) {
    if (ctx->wl == NULL) return;

    printf("[info] cleanup_wl: destroying wayland objects\n");

    output_list_node_t * cur = ctx->wl->outputs;
    output_list_node_t * prev = NULL;
    while (cur != NULL) {
        prev = cur;
        cur = cur->next;

        wl_output_destroy(prev->output);
        free(prev->name);
        free(prev);
    }
    ctx->wl->outputs = NULL;

    if (ctx->wl->xdg_toplevel != NULL) xdg_toplevel_destroy(ctx->wl->xdg_toplevel);
    if (ctx->wl->xdg_surface != NULL) xdg_surface_destroy(ctx->wl->xdg_surface);
    if (ctx->wl->surface != NULL) wl_surface_destroy(ctx->wl->surface);
    if (ctx->wl->dmabuf_manager != NULL) zwlr_export_dmabuf_manager_v1_destroy(ctx->wl->dmabuf_manager);
    if (ctx->wl->output_manager != NULL) zxdg_output_manager_v1_destroy(ctx->wl->output_manager);
    if (ctx->wl->wm_base != NULL) xdg_wm_base_destroy(ctx->wl->wm_base);
    if (ctx->wl->compositor != NULL) wl_compositor_destroy(ctx->wl->compositor);
    if (ctx->wl->registry != NULL) wl_registry_destroy(ctx->wl->registry);
    if (ctx->wl->display != NULL) wl_display_disconnect(ctx->wl->display);

    free(ctx->wl);
    ctx->wl = NULL;
}

