#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "context.h"

void cleanup(ctx_t * ctx) {
    log_debug(ctx, "cleanup: deallocating resources\n");
    if (ctx->mirror.initialized) cleanup_mirror(ctx);
    if (ctx->egl.initialized) cleanup_egl(ctx);
    if (ctx->wl.initialized) cleanup_wl(ctx);
    cleanup_opt(ctx);
}

void cleanup_opt(ctx_t * ctx) {
    if (ctx->opt.output != NULL) free(ctx->opt.output);
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

bool parse_region_option(region_t * region, char ** output, const char * region_arg) {
    region_t local_region = { .x = 0, .y = 0, .width = 0, .height = 0 };

    char * region_str = strdup(region_arg);
    if (region_str == NULL) {
        log_error("parse_region_option: failed to allocate copy of region argument\n");
        return false;
    }

    char * position = strtok(region_str, " ");
    char * size = strtok(NULL, " ");
    char * output_label = strtok(NULL, " ");

    if (position == NULL) {
        log_error("parse_region_option: missing region position\n");
        free(region_str);
        return false;
    }

    char * x = strtok(position, ",");
    char * y = strtok(NULL, ",");
    char * rest = strtok(NULL, ",");

    if (x == NULL) {
        log_error("parse_region_option: missing x position\n");
        free(region_str);
        return false;
    } else if (y == NULL) {
        log_error("parse_region_option: missing y position\n");
        free(region_str);
        return false;
    } else if (rest != NULL) {
        log_error("parse_region_option: unexpected position component %s\n", rest);
        free(region_str);
        return false;
    }

    char * end = NULL;
    local_region.x = strtoul(x, &end, 10);
    if (*end != '\0') {
        log_error("parse_region_option: invalid x position %s\n", x);
        free(region_str);
        return false;
    }

    end = NULL;
    local_region.y = strtoul(y, &end, 10);
    if (*end != '\0') {
        log_error("parse_region_option: invalid y position %s\n", y);
        free(region_str);
        return false;
    }

    if (size == NULL) {
        log_error("parse_region_option: missing size\n");
        free(region_str);
        return false;
    }

    char * width = strtok(size, "x");
    char * height = strtok(NULL, "x");
    rest = strtok(NULL, "x");
    if (width == NULL) {
        log_error("parse_region_option: missing width\n");
        free(region_str);
        return false;
    } else if (height == NULL) {
        log_error("parse_region_option: missing height\n");
        free(region_str);
        return false;
    } else if (rest != NULL) {
        log_error("parse_region_option: unexpected size component %s\n", rest);
        free(region_str);
        return false;
    }

    end = NULL;
    local_region.width = strtoul(width, &end, 10);
    if (*end != '\0') {
        log_error("parse_region_option: invalid width %s\n", width);
        free(region_str);
        return false;
    } else if (local_region.width == 0) {
        log_error("parse_region_option: invalid width %d\n", local_region.width);
        free(region_str);
        return false;
    }

    end = NULL;
    local_region.height = strtoul(height, &end, 10);
    if (*end != '\0') {
        log_error("parse_region_option: invalid height %s\n", height);
        free(region_str);
        return false;
    } else if (local_region.height == 0) {
        log_error("parse_region_option: invalid height %d\n", local_region.height);
        free(region_str);
        return false;
    }

    if (output_label != NULL) {
        *output = strdup(output_label);
        if (*output == NULL) {
            log_error("parse_region_option: failed to allocate copy of output name\n");
            return false;
        }
    }

    *region = local_region;
    free(region_str);
    return true;
}

static void usage(ctx_t * ctx) {
    printf("usage: wl-mirror [options] <output>\n");
    printf("\n");
    printf("options:\n");
    printf("  -h,   --help             show this help\n");
    printf("  -v,   --verbose          enable debug logging\n");
    printf("  -c,   --show-cursor      show the cursor on the mirrored screen (default)\n");
    printf("  -i,   --invert-colors    invert colors in the mirrored screen\n");
    printf("  -n,   --no-show-cursor   don't show the cursor on the mirrored screen\n");
    printf("  -s l, --scaling linear   use linear scaling (default)\n");
    printf("  -s n, --scaling nearest  use nearest neighbor scaling\n");
    printf("  -s e, --scaling exact    only scale to exact multiples of the output size\n");
    printf("  -t T, --transform T      apply custom transform T\n");
    printf("  -r R, --region R         capture custom region R\n");
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
    printf("\n");
    printf("regions:\n");
    printf("  regions are specified in the format used by the slurp utility\n");
    printf("  - '<x>,<y> <width>x<height> [output]'\n");
    printf("  on start, the region is translated into output coordinates\n");
    printf("  when the output moves, the captured region moves with it\n");
    printf("  when a region is specified, the <output> argument is optional\n");
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
    ctx.opt.invert_colors = false;
    ctx.opt.has_region = false;
    ctx.opt.scaling = SCALE_LINEAR;
    ctx.opt.transform = (transform_t){ .rotation = ROT_NORMAL, .flip_x = false, .flip_y = false };
    ctx.opt.output = NULL;

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
        } else if (strcmp(argv[0], "-i") == 0 || strcmp(argv[0], "--invert-colors") == 0) {
            ctx.opt.invert_colors = true;
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
                log_error("main: invalid region %s\n", argv[1]);
                exit_fail(&ctx);
            }

            argv++;
            argc--;
        } else if (strcmp(argv[0], "-r") == 0 || strcmp(argv[0], "--region") == 0) {
            if (argc < 2) {
                log_error("main: option %s requires an argument\n", argv[0]);
                exit_fail(&ctx);
            } else if (!parse_region_option(&ctx.opt.region, &ctx.opt.output, argv[1])) {
                log_error("main: invalid transform %s\n", argv[1]);
                exit_fail(&ctx);
            }

            ctx.opt.has_region = true;
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

    if (argc == 0 && ctx.opt.has_region && ctx.opt.output == NULL) {
        // output implicitly defined by region
    } else if (argc == 0 && ctx.opt.has_region && ctx.opt.output != NULL) {
        // output explicitly defined by region
    } else if (argc == 1 && ctx.opt.has_region && ctx.opt.output != NULL) {
        // output defined by both region and argument
        // must be the same
        if (strcmp(ctx.opt.output, argv[0]) != 0) {
            log_error("main: region and argument output differ: %s vs %s\n", ctx.opt.output, argv[0]);
            exit_fail(&ctx);
        }
    } else if (argc == 1) {
        // output defined by argument
        ctx.opt.output = strdup(argv[0]);
        if (ctx.opt.output == NULL) {
            log_error("main: failed to allocate copy of output name\n");
            exit_fail(&ctx);
        }
    } else {
        usage(&ctx);
    }

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
