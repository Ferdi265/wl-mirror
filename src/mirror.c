#include <stdio.h>
#include <stdlib.h>
#include "context.h"

// --- init_mirror ---

void init_mirror(ctx_t * ctx, char * output) {
    printf("[info] init_mirror: allocating context structure\n");
    ctx->mirror = malloc(sizeof (ctx_mirror_t));
    if (ctx->mirror == NULL) {
        printf("[error] init_wl: failed to allocate context structure\n");
        exit_fail(ctx);
    }

    ctx->mirror->output = output;
}

// --- output_added_handler_mirror ---

void output_added_handler_mirror(ctx_t * ctx, output_list_node_t * node) {

}

// --- output_removed_handler_mirror ---

void output_removed_handler_mirror(ctx_t * ctx, output_list_node_t * node) {

}

// --- configure_resize_handler_mirror ---

void configure_resize_handler_mirror(ctx_t * ctx, uint32_t width, uint32_t height) {

}

// --- cleanup_mirror ---

void cleanup_mirror(ctx_t *ctx) {
    if (ctx->mirror == NULL) return;

    printf("[info] cleanup_mirror: destroying mirror objects\n");

    free(ctx->mirror);
    ctx->mirror = NULL;
}
