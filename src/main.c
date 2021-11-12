#include <stdio.h>
#include <stdlib.h>
#include "context.h"

void cleanup(ctx_t * ctx) {
    log_debug("[debug] cleanup: deallocating resources\n");
    if (ctx == NULL) return;
    if (ctx->mirror != NULL) cleanup_mirror(ctx);
    if (ctx->egl != NULL) cleanup_egl(ctx);
    if (ctx->wl != NULL) cleanup_wl(ctx);
    free(ctx);
}

void exit_fail(ctx_t * ctx) {
    cleanup(ctx);
    exit(1);
}

int main(int argc, char ** argv) {
    if (argc != 2) {
        printf("usage: wl-mirror <output>\n");
        exit(1);
    }

    log_debug("[debug] main: allocating context structure\n");
    ctx_t * ctx = malloc(sizeof (ctx_t));
    if (ctx == NULL) {
        printf("[error] main: failed to allocate context structure\n");
        exit_fail(ctx);
    }

    ctx->wl = NULL;
    ctx->egl = NULL;
    ctx->mirror = NULL;

    log_debug("[debug] main: initializing wayland\n");
    init_wl(ctx);

    log_debug("[debug] main: initializing EGL\n");
    init_egl(ctx);

    log_debug("[debug] main: initializing mirror\n");
    char * output = argv[1];
    init_mirror(ctx, output);

    log_debug("[debug] main: entering event loop\n");
    while (wl_display_dispatch(ctx->wl->display) != -1 && !ctx->wl->closing) {}
    log_debug("[debug] main: exiting event loop\n");

    cleanup(ctx);
}
