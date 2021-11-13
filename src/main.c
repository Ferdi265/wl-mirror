#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "context.h"

void cleanup(ctx_t * ctx) {
    log_debug("[debug] cleanup: deallocating resources\n");
    if (ctx == NULL) return;
    if (ctx->mirror != NULL) cleanup_mirror(ctx);
    if (ctx->egl != NULL) cleanup_egl(ctx);
    if (ctx->wl != NULL) cleanup_wl(ctx);
    if (ctx->opt != NULL) free(ctx->opt);
    free(ctx);
}

void exit_fail(ctx_t * ctx) {
    cleanup(ctx);
    exit(1);
}

static void usage(ctx_t * ctx) {
    printf("usage: wl-mirror [options] <output>\n");
    printf("\n");
    printf("options:\n");
    printf("  -h,   --help             show this help\n");
    printf("  -c,   --show-cursor      show the cursor on the mirrored screen\n");
    printf("  -n,   --no-show-cursor   don't show the cursor on the mirrored screen\n");
    printf("  -s l, --scaling linear   use linear scaling\n");
    printf("  -s n, --scaling nearest  use nearest neighbor scaling\n");
    printf("  -s e, --scaling exact    only scale to exact multiples of the \n");
    cleanup(ctx);
    exit(0);
}

int main(int argc, char ** argv) {
    log_debug("[debug] main: allocating context structure\n");
    ctx_t * ctx = malloc(sizeof (ctx_t));
    if (ctx == NULL) {
        printf("[error] main: failed to allocate context structure\n");
        exit_fail(ctx);
    }

    ctx->opt = NULL;
    ctx->wl = NULL;
    ctx->egl = NULL;
    ctx->mirror = NULL;

    log_debug("[debug] main: allocating option context structure\n");
    ctx->opt = malloc(sizeof (ctx_opt_t));
    if (ctx->opt == NULL) {
        printf("[error] main: failed to allocate option context structure\n");
        exit_fail(ctx);
    }

    ctx->opt->show_cursor = true;
    ctx->opt->scaling = SCALE_LINEAR;

    if (argc > 0) {
        // skip program name
        argv++;
        argc--;
    }

    while (argc > 0 && argv[0][0] == '-') {
        if (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "--help") == 0) {
            usage(ctx);
        } else if (strcmp(argv[0], "-c") == 0 || strcmp(argv[0], "--show-cursor") == 0) {
            ctx->opt->show_cursor = true;
        } else if (strcmp(argv[0], "-n") == 0 || strcmp(argv[0], "--no-show-cursor") == 0) {
            ctx->opt->show_cursor = false;
        } else if (strcmp(argv[0], "-") == 0 || strcmp(argv[0], "--scaling") == 0) {
            if (argc < 2) {
                printf("[error] main: option %s requires an argument\n", argv[0]);
                exit_fail(ctx);
            } else if (strcmp(argv[1], "l") == 0 || strcmp(argv[1], "linear") == 0) {
                ctx->opt->scaling = SCALE_LINEAR;
            } else if (strcmp(argv[1], "n") == 0 || strcmp(argv[1], "nearest") == 0) {
                ctx->opt->scaling = SCALE_NEAREST;
            } else if (strcmp(argv[1], "e") == 0 || strcmp(argv[1], "exact") == 0) {
                ctx->opt->scaling = SCALE_EXACT;
            } else {
                printf("[error] main: invalid scaling mode %s\n", argv[1]);
                exit_fail(ctx);
            }

            argv++;
            argc--;
        } else if (strcmp(argv[0], "--") == 0) {
            argv++;
            argc--;
            break;
        } else {
            printf("[error] main: invalid option %s\n", argv[0]);
            exit_fail(ctx);
        }

        argv++;
        argc--;
    }

    if (argc != 1) {
        printf("usage: wl-mirror <output>\n");
        exit_fail(ctx);
    }
    char * output = argv[0];

    log_debug("[debug] main: initializing wayland\n");
    init_wl(ctx);

    log_debug("[debug] main: initializing EGL\n");
    init_egl(ctx);

    log_debug("[debug] main: initializing mirror\n");
    init_mirror(ctx, output);

    log_debug("[debug] main: entering event loop\n");
    while (wl_display_dispatch(ctx->wl->display) != -1 && !ctx->wl->closing) {}
    log_debug("[debug] main: exiting event loop\n");

    cleanup(ctx);
}
