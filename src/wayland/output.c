#include <stdlib.h>
#include <string.h>
#include "context.h"

static void check_outputs_complete(ctx_t * ctx) {
    if (!wayland_registry_is_initial_sync_complete(ctx)) return;
    if (ctx->wl.output.incomplete_outputs > 0) return;

    log_debug(ctx, "wayland::output::check_outputs_complete(): output information complete\n");
    wayland_events_emit_output_initial_sync(ctx);
}

static wayland_output_entry_t * find_output(ctx_t * ctx, struct wl_output * output) {
    bool found = false;
    wayland_output_entry_t * cur;
    wl_list_for_each(cur, &ctx->wl.output.output_list, link) {
        if (cur->output == output) {
            found = true;
            break;
        }
    }

    if (!found) {
        log_error("wayland::output::find_output(): could not find output entry for output %p\n", output);
        exit_fail(ctx);
    }

    return cur;
}

static void on_output_name(
    void * data, struct wl_output * output,
    const char * name
) {
    if (output == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    wayland_output_entry_t * cur = find_output(ctx, output);

    if (cur->name == NULL || strcmp(cur->name, name) != 0) {
        if (cur->name != NULL) free(cur->name);
        cur->name = strdup(name);
        cur->changed |= WAYLAND_OUTPUT_NAME_CHANGED;
    }
}

static void on_output_description(
    void * data, struct wl_output * output,
    const char * description
) {
    // ignore: description information not needed
    (void)data;
    (void)output;
    (void)description;
}

static void on_output_mode(
    void * data, struct wl_output * output,
    uint32_t flags, int32_t width, int32_t height, int32_t refresh
) {
    // ignore: mode information not needed
    (void)data;
    (void)output;
    (void)flags;
    (void)width;
    (void)height;
    (void)refresh;
}

static void on_output_geometry(
    void * data, struct wl_output * output,
    int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
    int32_t subpixel, const char * make, const char * model, int32_t transform_int
) {
    if (output == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    wayland_output_entry_t * cur = find_output(ctx, output);

    enum wl_output_transform transform = (enum wl_output_transform)transform_int;
    if (cur->transform != transform) {
        cur->transform = transform;
        cur->changed |= WAYLAND_OUTPUT_TRANSFORM_CHANGED;
    }

    // ignore: geometry information is often stubbed out, use xdg_output information
    (void)x;
    (void)y;
    (void)physical_width;
    (void)physical_height;
    (void)subpixel;
    (void)make;
    (void)model;
}

static void on_output_scale(
    void * data, struct wl_output * output,
    int32_t scale
) {
    if (output == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    wayland_output_entry_t * cur = find_output(ctx, output);

    if (cur->scale != scale) {
        cur->scale = scale;
        cur->changed |= WAYLAND_OUTPUT_SCALE_CHANGED;
    }
}

static void on_output_done(
    void * data, struct wl_output * output
) {
    if (output == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    wayland_output_entry_t * cur = find_output(ctx, output);

    if (cur->incomplete == WAYLAND_OUTPUT_XDG_COMPLETE) {
        cur->incomplete = WAYLAND_OUTPUT_COMPLETE;
        ctx->wl.output.incomplete_outputs--;

        check_outputs_complete(ctx);
    }

    // only emit change info for complete outputs
    if (!cur->incomplete && cur->changed) {
        wayland_events_emit_output_changed(ctx, cur);
        cur->changed = WAYLAND_OUTPUT_UNCHANGED;
    }
}

const struct wl_output_listener output_listener = {
    .name = on_output_name,
    .description = on_output_description,
    .mode = on_output_mode,
    .geometry = on_output_geometry,
    .scale = on_output_scale,
    .done = on_output_done
};

static wayland_output_entry_t * find_xdg_output(ctx_t * ctx, struct zxdg_output_v1 * xdg_output) {
    bool found = false;
    wayland_output_entry_t * cur;
    wl_list_for_each(cur, &ctx->wl.output.output_list, link) {
        if (cur->xdg_output == xdg_output) {
            found = true;
            break;
        }
    }

    if (!found) {
        log_error("wayland::output::find_xdg_output(): could not find output entry for xdg_output %p\n", xdg_output);
        exit_fail(ctx);
    }

    return cur;
}

static void on_xdg_output_name(
    void * data, struct zxdg_output_v1 * xdg_output,
    const char * name
) {
    // ignore: event is deprecated in favor of on_output_name
    (void)data;
    (void)xdg_output;
    (void)name;
}

static void on_xdg_output_description(
    void * data, struct zxdg_output_v1 * xdg_output,
    const char * description
) {
    // ignore: event is deprecated in favor of on_output_description
    (void)data;
    (void)xdg_output;
    (void)description;
}

static void on_xdg_output_logical_position(
    void * data, struct zxdg_output_v1 * xdg_output,
    int32_t x, int32_t y
) {
    if (xdg_output == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    wayland_output_entry_t * cur = find_xdg_output(ctx, xdg_output);

    if (cur->x != x || cur->y != y) {
        cur->x = x;
        cur->y = y;
        cur->changed |= WAYLAND_OUTPUT_POSITION_CHANGED;
    }

    if (cur->incomplete) {
        cur->incomplete = WAYLAND_OUTPUT_XDG_COMPLETE;
    }
}

static void on_xdg_output_logical_size(
    void * data, struct zxdg_output_v1 * xdg_output,
    int32_t width, int32_t height
) {
    if (xdg_output == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    wayland_output_entry_t * cur = find_xdg_output(ctx, xdg_output);

    if (cur->width != width || cur->height != height) {
        cur->width = width;
        cur->height = height;
        cur->changed |= WAYLAND_OUTPUT_POSITION_CHANGED;
    }

    if (cur->incomplete) {
        cur->incomplete = WAYLAND_OUTPUT_XDG_COMPLETE;
    }
}

static void on_xdg_output_done(
    void * data, struct zxdg_output_v1 * xdg_output
) {
    // ignore: event is deprecated in favor of on_output_done
    (void)data;
    (void)xdg_output;
}

const struct zxdg_output_v1_listener xdg_output_listener = {
    .name = on_xdg_output_name,
    .description = on_xdg_output_description,
    .logical_position = on_xdg_output_logical_position,
    .logical_size = on_xdg_output_logical_size,
    .done = on_xdg_output_done
};

static void bind_xdg_output(ctx_t * ctx, wayland_output_entry_t * cur) {
    cur->xdg_output = zxdg_output_manager_v1_get_xdg_output(ctx->wl.protocols.xdg_output_manager, cur->output);
    zxdg_output_v1_add_listener(cur->xdg_output, &xdg_output_listener, (void *)ctx);

    (void)ctx;
}

void wayland_output_zero(ctx_t * ctx) {
    wl_list_init(&ctx->wl.output.output_list);

    ctx->wl.output.incomplete_outputs = 0;
}

void wayland_output_init(ctx_t * ctx) {
    (void)ctx;
}

void wayland_output_cleanup(ctx_t * ctx) {
    wayland_output_entry_t *cur, *next;
    wl_list_for_each_safe(cur, next, &ctx->wl.output.output_list, link) {
        wl_list_remove(&cur->link);
        if (cur->xdg_output != NULL) zxdg_output_v1_destroy(cur->xdg_output);
        if (cur->name != NULL) free(cur->name);
        free(cur);
    }
}

void wayland_output_on_add(ctx_t * ctx, struct wl_output * output) {
    wl_output_add_listener(output, &output_listener, (void *)ctx);

    wayland_output_entry_t * cur = malloc(sizeof *cur);
    cur->output = output;
    cur->xdg_output = NULL;
    cur->name = NULL;
    cur->x = 0;
    cur->y = 0;
    cur->width = 0;
    cur->height = 0;
    cur->scale = 0;
    cur->transform = WL_OUTPUT_TRANSFORM_NORMAL;
    cur->changed = WAYLAND_OUTPUT_CHANGED;
    cur->incomplete = WAYLAND_OUTPUT_INCOMPLETE;
    wl_list_insert(&ctx->wl.output.output_list, &cur->link);

    ctx->wl.output.incomplete_outputs++;

    if (ctx->wl.protocols.xdg_output_manager != NULL) {
        bind_xdg_output(ctx, cur);
    }
}

void wayland_output_on_remove(ctx_t * ctx, struct wl_output * output) {
    wayland_output_entry_t *cur, *next;
    wl_list_for_each_safe(cur, next, &ctx->wl.output.output_list, link) {
        if (cur->output != output) continue;

        wl_list_remove(&cur->link);
        if (cur->xdg_output != NULL) zxdg_output_v1_destroy(cur->xdg_output);
        if (cur->name != NULL) free(cur->name);
        free(cur);
    }
}

void wayland_output_on_initial_sync(ctx_t * ctx) {
    wayland_output_entry_t *cur;
    wl_list_for_each(cur, &ctx->wl.output.output_list, link) {
        if (cur->xdg_output == NULL) {
            bind_xdg_output(ctx, cur);
        }
    }

    check_outputs_complete(ctx);
}
