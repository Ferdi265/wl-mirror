#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "context.h"

// --- export_dmabuf_frame event handlers ---

// TODO

static const struct zwlr_export_dmabuf_frame_v1_listener frame_listener = {
    // TODO
};

// --- init_mirror ---

void init_mirror(ctx_t * ctx, char * output) {
    printf("[info] init_mirror: allocating context structure\n");
    ctx->mirror = malloc(sizeof (ctx_mirror_t));
    if (ctx->mirror == NULL) {
        printf("[error] init_wl: failed to allocate context structure\n");
        exit_fail(ctx);
    }

    ctx->mirror->current = NULL;
    ctx->mirror->frame = NULL;

    printf("[info] init_mirror: searching for output\n");
    output_list_node_t * cur = ctx->wl->outputs;
    while (cur != NULL) {
        if (strcmp(cur->name, output) == 0) {
            ctx->mirror->current = cur;
            break;
        }

        cur = cur->next;
    }

    if (ctx->mirror->current == NULL) {
        printf("[error] init_mirror: output %s not found\n", output);
        exit_fail(ctx);
    } else {
        printf("[info] init_mirror: found output with name %s\n", output);
    }

    printf("[info] init_mirror: formatting window title\n");
    char * title = NULL;
    asprintf(&title, "Wayland Output Mirror for %s", output);
    if (title == NULL) {
        printf("[error] init_mirror: failed to format window title\n");
        exit_fail(ctx);
    }

    printf("[info] init_mirror: setting window title\n");
    xdg_toplevel_set_title(ctx->wl->xdg_toplevel, title);
    free(title);

    printf("[info] init_mirror: creating wlr_dmabuf_export_frame\n");
    ctx->mirror->frame = zwlr_export_dmabuf_manager_v1_capture_output(
        ctx->wl->dmabuf_manager, true, ctx->mirror->current->output
    );
    if (ctx->mirror->frame == NULL) {
        printf("[error] init_mirror: failed to create wlr_dmabuf_export_frame\n");
        exit_fail(ctx);
    }

    printf("[info] init_mirror: adding wlr_dmabuf_export_frame event listener\n");
    // TODO
}

// --- output_removed_handler_mirror ---

void output_removed_handler_mirror(ctx_t * ctx, output_list_node_t * node) {
    if (ctx->mirror == NULL) return;
    if (ctx->mirror->current == NULL) return;
    if (ctx->mirror->current != node) return;

    printf("[error] output_removed_handler_mirror: output disappeared, closing\n");
    exit_fail(ctx);
}

// --- configure_resize_handler_mirror ---

void configure_resize_handler_mirror(ctx_t * ctx, uint32_t width, uint32_t height) {
    // TODO
}

// --- cleanup_mirror ---

void cleanup_mirror(ctx_t *ctx) {
    if (ctx->mirror == NULL) return;

    printf("[info] cleanup_mirror: destroying mirror objects\n");
    if (ctx->mirror->frame != NULL) zwlr_export_dmabuf_frame_v1_destroy(ctx->mirror->frame);

    free(ctx->mirror);
    ctx->mirror = NULL;
}
