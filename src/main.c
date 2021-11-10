#include <stdio.h>
#include <stdlib.h>
#include "context.h"

void cleanup(ctx_t * ctx) {
    printf("[info] cleanup: deallocating resources\n");
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

    printf("[info] main: allocating context structure\n");
    ctx_t * ctx = malloc(sizeof (ctx_t));
    if (ctx == NULL) {
        printf("[error] main: failed to allocate context structure\n");
        exit_fail(ctx);
    }

    ctx->wl = NULL;
    ctx->egl = NULL;
    ctx->mirror = NULL;

    printf("[info] main: initializing wayland\n");
    init_wl(ctx);

    printf("[info] main: initializing EGL\n");
    init_egl(ctx);

    printf("[info] main: initializing mirror\n");
    char * output = argv[1];
    init_mirror(ctx, output);

    cleanup(ctx);
}
