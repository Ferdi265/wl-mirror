#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "context.h"

void cleanup(ctx_t * ctx) {
    log_debug(ctx, "cleanup: deallocating resources\n");
    if (ctx->mirror.initialized) cleanup_mirror(ctx);
    if (ctx->egl.initialized) cleanup_egl(ctx);
    if (ctx->wl.initialized) cleanup_wl(ctx);
}

void exit_fail(ctx_t * ctx) {
    cleanup(ctx);
    exit(1);
}

bool parse_scaling_option(scale_t * scaling, const char * scaling_arg) {
    if (strcmp(scaling_arg, "l") == 0 || strcmp(scaling_arg, "linear") == 0) {
        *scaling = SCALE_LINEAR;
        return true;
    } else if (strcmp(scaling_arg, "n") == 0 || strcmp(scaling_arg, "nearest") == 0) {
        *scaling = SCALE_NEAREST;
        return true;
    } else if (strcmp(scaling_arg, "e") == 0 || strcmp(scaling_arg, "exact") == 0) {
        *scaling = SCALE_EXACT;
        return true;
    } else {
        return false;
    }
}

bool parse_transform_option(transform_t * transform, const char * transform_arg) {
    transform_t local_transform = { .rotation = ROT_NORMAL, .flip_x = false, .flip_y = false };

    if (strcmp(transform_arg, "normal") == 0) {
        *transform = local_transform;
        return true;
    }

    char * transform_str = strdup(transform_arg);
    if (transform_str == NULL) {
        log_error("parse_transform_option: failed to allocate copy of transform argument\n");
        return false;
    }

    bool success = true;
    bool has_rotation = false;
    char * transform_spec = strtok(transform_str, "-");
    while (transform_spec != NULL) {
        if (strcmp(transform_spec, "normal") == 0) {
            log_error("parse_transform_option: %s must be the only transform specifier\n", transform_spec);
            success = false;
            break;
        } else if (strcmp(transform_spec, "flipX") == 0 || strcmp(transform_spec, "flipped") == 0) {
            if (local_transform.flip_x) {
                log_error("parse_transform_option: duplicate flip specifier %s\n", transform_spec);
                success = false;
                break;
            }

            local_transform.flip_x = true;
        } else if (strcmp(transform_spec, "flipY") == 0) {
            if (local_transform.flip_y) {
                log_error("parse_transform_option: duplicate flip specifier %s\n", transform_spec);
                success = false;
                break;
            }

            local_transform.flip_y = true;
        } else if (strcmp(transform_spec, "0") == 0 || strcmp(transform_spec, "0cw") == 0 || strcmp(transform_spec, "0ccw") == 0) {
            if (has_rotation) {
                log_error("parse_transform_option: duplicate rotation specifier %s\n", transform_spec);
                success = false;
                break;
            }

            has_rotation = true;
            local_transform.rotation = ROT_NORMAL;
        } else if (strcmp(transform_spec, "90") == 0 || strcmp(transform_spec, "90cw") == 0 || strcmp(transform_spec, "270ccw") == 0) {
            if (has_rotation) {
                log_error("parse_transform_option: duplicate rotation specifier %s\n", transform_spec);
                success = false;
                break;
            }

            has_rotation = true;
            local_transform.rotation = ROT_CW_90;
        } else if (strcmp(transform_spec, "180") == 0 || strcmp(transform_spec, "180cw") == 0 || strcmp(transform_spec, "180ccw") == 0) {
            if (has_rotation) {
                log_error("parse_transform_option: duplicate rotation specifier %s\n", transform_spec);
                success = false;
                break;
            }

            has_rotation = true;
            local_transform.rotation = ROT_CW_180;
        } else if (strcmp(transform_spec, "270") == 0 || strcmp(transform_spec, "270cw") == 0 || strcmp(transform_spec, "90ccw") == 0) {
            if (has_rotation) {
                log_error("parse_transform_option: duplicate rotation specifier %s\n", transform_spec);
                success = false;
                break;
            }

            has_rotation = true;
            local_transform.rotation = ROT_CW_270;
        } else {
            log_error("parse_transform_option: invalid transform specifier %s\n", transform_spec);
            success = false;
            break;
        }

        transform_spec = strtok(NULL, "-");
    }

    *transform = local_transform;
    free(transform_str);
    return success;
}

static void usage(ctx_t * ctx) {
    printf("usage: wl-mirror [options] <output>\n");
    printf("\n");
    printf("options:\n");
    printf("  -h,   --help             show this help\n");
    printf("  -v,   --verbose          enable debug logging\n");
    printf("  -c,   --show-cursor      show the cursor on the mirrored screen (default)\n");
    printf("  -n,   --no-show-cursor   don't show the cursor on the mirrored screen\n");
    printf("  -s l, --scaling linear   use linear scaling (default)\n");
    printf("  -s n, --scaling nearest  use nearest neighbor scaling\n");
    printf("  -s e, --scaling exact    only scale to exact multiples of the output size\n");
    printf("  -t T, --transform T      apply custom transform T\n");
    printf("\n");
    printf("transforms:\n");
    printf("  transforms are specified as a dash-separated list of flips followed by a rotation\n");
    printf("  flips are applied before rotations\n");
    printf("  - normal                         no transformation\n");
    printf("  - flipX, flipY                   flip the X or Y coordinate\n");
    printf("  - 0cw,  90cw,  180cw,  270cw     apply a clockwise rotation\n");
    printf("  - 0ccw, 90ccw, 180ccw, 270ccw    apply a counter-clockwise rotation\n");
    printf("  the following transformation options are provided for compatibility with sway output transforms\n");
    printf("  - flipped                        flip the X coordinate\n");
    printf("  - 0,    90,    180,    270       apply a clockwise rotation\n");
    cleanup(ctx);
    exit(0);
}

int main(int argc, char ** argv) {
    ctx_t ctx;

    ctx.wl.initialized = false;
    ctx.egl.initialized = false;
    ctx.mirror.initialized = false;

    ctx.opt.verbose = false;
    ctx.opt.show_cursor = true;
    ctx.opt.scaling = SCALE_LINEAR;
    ctx.opt.transform = (transform_t){ .rotation = ROT_NORMAL, .flip_x = false, .flip_y = false };

    if (argc > 0) {
        // skip program name
        argv++;
        argc--;
    }

    while (argc > 0 && argv[0][0] == '-') {
        if (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "--help") == 0) {
            usage(&ctx);
        } else if (strcmp(argv[0], "-v") == 0 || strcmp(argv[0], "--verbose") == 0) {
            ctx.opt.verbose = true;
        } else if (strcmp(argv[0], "-c") == 0 || strcmp(argv[0], "--show-cursor") == 0) {
            ctx.opt.show_cursor = true;
        } else if (strcmp(argv[0], "-n") == 0 || strcmp(argv[0], "--no-show-cursor") == 0) {
            ctx.opt.show_cursor = false;
        } else if (strcmp(argv[0], "-s") == 0 || strcmp(argv[0], "--scaling") == 0) {
            if (argc < 2) {
                log_error("main: option %s requires an argument\n", argv[0]);
                exit_fail(&ctx);
            } else if (!parse_scaling_option(&ctx.opt.scaling, argv[1])) {
                log_error("main: invalid scaling mode %s\n", argv[1]);
                exit_fail(&ctx);
            }

            argv++;
            argc--;
        } else if (strcmp(argv[0], "-t") == 0 || strcmp(argv[0], "--transform") == 0) {
            if (argc < 2) {
                log_error("main: option %s requires an argument\n", argv[0]);
                exit_fail(&ctx);
            } else if (!parse_transform_option(&ctx.opt.transform, argv[1])) {
                log_error("main: invalid transform %s\n", argv[1]);
                exit_fail(&ctx);
            }

            argv++;
            argc--;
        } else if (strcmp(argv[0], "--") == 0) {
            argv++;
            argc--;
            break;
        } else {
            log_error("main: invalid option %s\n", argv[0]);
            exit_fail(&ctx);
        }

        argv++;
        argc--;
    }

    if (argc != 1) {
        usage(&ctx);
    }

    ctx.opt.output = argv[0];

    log_debug(&ctx, "main: initializing wayland\n");
    init_wl(&ctx);

    log_debug(&ctx, "main: initializing EGL\n");
    init_egl(&ctx);

    log_debug(&ctx, "main: initializing mirror\n");
    init_mirror(&ctx);

    log_debug(&ctx, "main: entering event loop\n");
    while (wl_display_dispatch(ctx.wl.display) != -1 && !ctx.wl.closing) {}
    log_debug(&ctx, "main: exiting event loop\n");

    cleanup(&ctx);
}
