#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client-protocol.h>
#include "context.h"

void cleanup_wl(ctx_t *ctx) {
    if (ctx->wl == NULL) return;

    printf("[info] cleanup_wl: destroying wayland objects\n");
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

// --- wl_registry event handlers ---

static void registry_event_add(
    void * data, struct wl_registry * registry,
    uint32_t id, const char * interface, uint32_t version
) {
    ctx_t * ctx = (ctx_t *)data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        if (ctx->wl->compositor != NULL) {
            printf("[error] wl_registry: duplicate compositor\n");
            exit_fail(ctx);
        }

        printf("[info] wl_registry: binding compositor\n");
        ctx->wl->compositor = (struct wl_compositor *)wl_registry_bind(
            registry, id,
            &wl_compositor_interface, wl_compositor_interface.version
        );
        ctx->wl->compositor_id = id;
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        if (ctx->wl->wm_base != NULL) {
            printf("[error] wl_registry: duplicate wm_base\n");
            exit_fail(ctx);
        }

        printf("[info] wl_registry: binding wm_base\n");
        ctx->wl->wm_base = (struct xdg_wm_base *)wl_registry_bind(
            registry, id,
            &xdg_wm_base_interface, xdg_wm_base_interface.version
        );
        ctx->wl->wm_base_id = id;
    } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
        if (ctx->wl->output_manager != NULL) {
            printf("[error] wl_registry: duplicate output_manager\n");
            exit_fail(ctx);
        }

        printf("[info] wl_registry: binding output_manager\n");
        ctx->wl->output_manager = (struct zxdg_output_manager_v1 *)wl_registry_bind(
            registry, id,
            &zxdg_output_manager_v1_interface, zxdg_output_manager_v1_interface.version
        );
        ctx->wl->output_manager_id = id;
    } else if (strcmp(interface, zwlr_export_dmabuf_manager_v1_interface.name) == 0) {
        if (ctx->wl->dmabuf_manager != NULL) {
            printf("[error] wl_registry: duplicate dmabuf_manager\n");
            exit_fail(ctx);
        }

        printf("[info] wl_registry: binding dmabuf_manager\n");
        ctx->wl->dmabuf_manager = (struct zwlr_export_dmabuf_manager_v1 *)wl_registry_bind(
            registry, id,
            &zwlr_export_dmabuf_manager_v1_interface, zwlr_export_dmabuf_manager_v1_interface.version
        );
        ctx->wl->dmabuf_manager_id = id;
    }
}

static void registry_event_remove(
    void * data, struct wl_registry * registry,
    uint32_t id
) {
    ctx_t * ctx = (ctx_t *)data;

    if (id == ctx->wl->compositor_id) {
        printf("[error] wl_registry: compositor disapperared\n");
        exit_fail(ctx);
    } else if (id == ctx->wl->wm_base_id) {
        printf("[error] wl_registry: wm_base disapperared\n");
        exit_fail(ctx);
    } else if (id == ctx->wl->output_manager_id) {
        printf("[error] wl_registry: output_manager disapperared\n");
        exit_fail(ctx);
    } else if (id == ctx->wl->dmabuf_manager_id) {
        printf("[error] wl_registry: dmabuf_manager disapperared\n");
        exit_fail(ctx);
    }

    (void)registry;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_event_add,
    .global_remove = registry_event_remove
};

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

    ctx->wl->surface = NULL;
    ctx->wl->xdg_surface = NULL;
    ctx->wl->xdg_toplevel = NULL;

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
    }
    if (ctx->wl->output_manager == NULL) {
        printf("[error] init_wl: output_manager missing\n");
        exit_fail(ctx);
    }
    if (ctx->wl->dmabuf_manager == NULL) {
        printf("[error] init_wl: dmabuf_manager missing\n");
        exit_fail(ctx);
    }
}
