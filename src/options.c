#include <stdlib.h>
#include <string.h>

#include "context.h"

void init_opt(ctx_t * ctx) {
    ctx->opt.verbose = false;
    ctx->opt.stream = false;
    ctx->opt.show_cursor = true;
    ctx->opt.invert_colors = false;
    ctx->opt.has_region = false;
    ctx->opt.scaling = SCALE_LINEAR;
    ctx->opt.backend = BACKEND_AUTO;
    ctx->opt.transform = (transform_t){ .rotation = ROT_NORMAL, .flip_x = false, .flip_y = false };
    ctx->opt.region = (region_t){ .x = 0, .y = 0, .width = 0, .height = 0 };
    ctx->opt.output = NULL;
}

void cleanup_opt(ctx_t * ctx) {
    if (ctx->opt.output != NULL) free(ctx->opt.output);
}

bool parse_scaling_opt(scale_t * scaling, const char * scaling_arg) {
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

bool parse_backend_opt(backend_t * backend, const char * backend_arg) {
    if (strcmp(backend_arg, "auto") == 0) {
        *backend = BACKEND_AUTO;
        return true;
    } else if (strcmp(backend_arg, "dmabuf") == 0) {
        *backend = BACKEND_DMABUF;
        return true;
    } else if (strcmp(backend_arg, "screencopy") == 0) {
        *backend = BACKEND_SCREENCOPY;
        return true;
    } else {
        return false;
    }
}

bool parse_transform_opt(transform_t * transform, const char * transform_arg) {
    transform_t local_transform = { .rotation = ROT_NORMAL, .flip_x = false, .flip_y = false };

    if (strcmp(transform_arg, "normal") == 0) {
        *transform = local_transform;
        return true;
    }

    char * transform_str = strdup(transform_arg);
    if (transform_str == NULL) {
        log_error("options::parse_transform_option(): failed to allocate copy of transform argument\n");
        return false;
    }

    bool success = true;
    bool has_rotation = false;
    char * transform_spec = strtok(transform_str, "-");
    while (transform_spec != NULL) {
        if (strcmp(transform_spec, "normal") == 0) {
            log_error("options::parse_transform_option(): %s must be the only transform specifier\n", transform_spec);
            success = false;
            break;
        } else if (strcmp(transform_spec, "flipX") == 0 || strcmp(transform_spec, "flipped") == 0) {
            if (local_transform.flip_x) {
                log_error("options::parse_transform_option(): duplicate flip specifier %s\n", transform_spec);
                success = false;
                break;
            }

            local_transform.flip_x = true;
        } else if (strcmp(transform_spec, "flipY") == 0) {
            if (local_transform.flip_y) {
                log_error("options::parse_transform_option(): duplicate flip specifier %s\n", transform_spec);
                success = false;
                break;
            }

            local_transform.flip_y = true;
        } else if (strcmp(transform_spec, "0") == 0 || strcmp(transform_spec, "0cw") == 0 || strcmp(transform_spec, "0ccw") == 0) {
            if (has_rotation) {
                log_error("options::parse_transform_option(): duplicate rotation specifier %s\n", transform_spec);
                success = false;
                break;
            }

            has_rotation = true;
            local_transform.rotation = ROT_NORMAL;
        } else if (strcmp(transform_spec, "90") == 0 || strcmp(transform_spec, "90cw") == 0 || strcmp(transform_spec, "270ccw") == 0) {
            if (has_rotation) {
                log_error("options::parse_transform_option(): duplicate rotation specifier %s\n", transform_spec);
                success = false;
                break;
            }

            has_rotation = true;
            local_transform.rotation = ROT_CW_90;
        } else if (strcmp(transform_spec, "180") == 0 || strcmp(transform_spec, "180cw") == 0 || strcmp(transform_spec, "180ccw") == 0) {
            if (has_rotation) {
                log_error("options::parse_transform_option(): duplicate rotation specifier %s\n", transform_spec);
                success = false;
                break;
            }

            has_rotation = true;
            local_transform.rotation = ROT_CW_180;
        } else if (strcmp(transform_spec, "270") == 0 || strcmp(transform_spec, "270cw") == 0 || strcmp(transform_spec, "90ccw") == 0) {
            if (has_rotation) {
                log_error("options::parse_transform_option(): duplicate rotation specifier %s\n", transform_spec);
                success = false;
                break;
            }

            has_rotation = true;
            local_transform.rotation = ROT_CW_270;
        } else {
            log_error("options::parse_transform_option(): invalid transform specifier %s\n", transform_spec);
            success = false;
            break;
        }

        transform_spec = strtok(NULL, "-");
    }

    if (success) {
        *transform = local_transform;
    }

    free(transform_str);
    return success;
}

bool parse_region_opt(region_t * region, char ** output, const char * region_arg) {
    region_t local_region = { .x = 0, .y = 0, .width = 0, .height = 0 };

    char * region_str = strdup(region_arg);
    if (region_str == NULL) {
        log_error("options::parse_region_option(): failed to allocate copy of region argument\n");
        return false;
    }

    char * position = strtok(region_str, " ");
    char * size = strtok(NULL, " ");
    char * output_label = strtok(NULL, " ");

    if (position == NULL) {
        log_error("options::parse_region_option(): missing region position\n");
        free(region_str);
        return false;
    }

    char * x = strtok(position, ",");
    char * y = strtok(NULL, ",");
    char * rest = strtok(NULL, ",");

    if (x == NULL) {
        log_error("options::parse_region_option(): missing x position\n");
        free(region_str);
        return false;
    } else if (y == NULL) {
        log_error("options::parse_region_option(): missing y position\n");
        free(region_str);
        return false;
    } else if (rest != NULL) {
        log_error("options::parse_region_option(): unexpected position component %s\n", rest);
        free(region_str);
        return false;
    }

    char * end = NULL;
    local_region.x = strtoul(x, &end, 10);
    if (*end != '\0') {
        log_error("options::parse_region_option(): invalid x position %s\n", x);
        free(region_str);
        return false;
    }

    end = NULL;
    local_region.y = strtoul(y, &end, 10);
    if (*end != '\0') {
        log_error("options::parse_region_option(): invalid y position %s\n", y);
        free(region_str);
        return false;
    }

    if (size == NULL) {
        log_error("options::parse_region_option(): missing size\n");
        free(region_str);
        return false;
    }

    char * width = strtok(size, "x");
    char * height = strtok(NULL, "x");
    rest = strtok(NULL, "x");
    if (width == NULL) {
        log_error("options::parse_region_option(): missing width\n");
        free(region_str);
        return false;
    } else if (height == NULL) {
        log_error("options::parse_region_option(): missing height\n");
        free(region_str);
        return false;
    } else if (rest != NULL) {
        log_error("options::parse_region_option(): unexpected size component %s\n", rest);
        free(region_str);
        return false;
    }

    end = NULL;
    local_region.width = strtoul(width, &end, 10);
    if (*end != '\0') {
        log_error("options::parse_region_option(): invalid width %s\n", width);
        free(region_str);
        return false;
    } else if (local_region.width == 0) {
        log_error("options::parse_region_option(): invalid width %d\n", local_region.width);
        free(region_str);
        return false;
    }

    end = NULL;
    local_region.height = strtoul(height, &end, 10);
    if (*end != '\0') {
        log_error("options::parse_region_option(): invalid height %s\n", height);
        free(region_str);
        return false;
    } else if (local_region.height == 0) {
        log_error("options::parse_region_option(): invalid height %d\n", local_region.height);
        free(region_str);
        return false;
    }

    if (output_label != NULL) {
        *output = strdup(output_label);
        if (*output == NULL) {
            log_error("options::parse_region_option(): failed to allocate copy of output name\n");
            free(region_str);
            return false;
        }
    }

    *region = local_region;
    free(region_str);
    return true;
}

bool find_output_opt(ctx_t * ctx, output_list_node_t ** output_handle, region_t * region_handle) {
    char * output_name = ctx->opt.output;
    output_list_node_t * local_output_handle = NULL;
    region_t local_region = (region_t){ .x = 0, .y = 0, .width = 0, .height = 0 };

    if (ctx->opt.output != NULL) {
        log_debug(ctx, "options::find_output(): searching for output by name\n");
        output_list_node_t * cur = ctx->wl.outputs;
        while (cur != NULL) {
            if (cur->name != NULL && strcmp(cur->name, ctx->opt.output) == 0) {
                local_output_handle = cur;
                output_name = cur->name;
                break;
            }

            cur = cur->next;
        }
    } else if (ctx->opt.has_region) {
        log_debug(ctx, "options::find_output(): searching for output by region\n");
        output_list_node_t * cur = ctx->wl.outputs;
        while (cur != NULL) {
            region_t output_region = {
                .x = cur->x, .y = cur->y,
                .width = cur->width, .height = cur->height
            };
            if (region_contains(&ctx->opt.region, &output_region)) {
                local_output_handle = cur;
                output_name = cur->name;
                break;
            }

            cur = cur->next;
        }
    }

    if (local_output_handle == NULL && ctx->opt.output != NULL) {
        log_error("options::find_output(): output %s not found\n", ctx->opt.output);
        return false;
    } else if (local_output_handle == NULL && ctx->opt.has_region) {
        log_error("options::find_output(): output for region not found\n");
        return false;
    } else if (local_output_handle == NULL) {
        log_error("options::find_output(): no output or region specified\n");
        return false;
    } else {
        log_debug(ctx, "options::find_output(): found output with name %s\n", output_name);
    }

    if (ctx->opt.has_region) {
        log_debug(ctx, "options::find_output(): checking if region in output\n");
        region_t output_region = {
            .x = local_output_handle->x, .y = local_output_handle->y,
            .width = local_output_handle->width, .height = local_output_handle->height
        };
        if (!region_contains(&ctx->opt.region, &output_region)) {
            log_error("options::find_output(): output does not contain region\n");
            return false;
        }

        log_debug(ctx, "options::find_output(): clamping region to output bounds\n");
        local_region = ctx->opt.region;
        region_clamp(&local_region, &output_region);
    }

    *output_handle = local_output_handle;
    *region_handle = local_region;
    return true;
}

void usage_opt(ctx_t * ctx) {
    printf("usage: wl-mirror [options] <output>\n");
    printf("\n");
    printf("options:\n");
    printf("  -h,   --help             show this help\n");
    printf("  -v,   --verbose          enable debug logging\n");
    printf("        --no-verbose       disable debug logging (default)\n");
    printf("  -c,   --show-cursor      show the cursor on the mirrored screen (default)\n");
    printf("        --no-show-cursor   don't show the cursor on the mirrored screen\n");
    printf("  -i,   --invert-colors    invert colors in the mirrored screen\n");
    printf("        --no-invert-colors don't invert colors in the mirrored screen (default)\n");
    printf("  -s l, --scaling linear   use linear scaling (default)\n");
    printf("  -s n, --scaling nearest  use nearest neighbor scaling\n");
    printf("  -s e, --scaling exact    only scale to exact multiples of the output size\n");
    printf("  -b B  --backend B        use a specific backend for capturing the screen\n");
    printf("  -t T, --transform T      apply custom transform T\n");
    printf("  -r R, --region R         capture custom region R\n");
    printf("        --no-region        capture the entire output (default)\n");
    printf("  -S,   --stream           accept a stream of additional options on stdin\n");
    printf("\n");
    printf("backends:\n");
    printf("  - dmabuf      use the wlr-export-dmabuf-unstable-v1 protocol to capture outputs\n");
    printf("  - screencopy  use the wlr-screencopy-unstable-v1 protocol to capture outputs\n");
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

void parse_opt(ctx_t * ctx, int argc, char ** argv) {
    bool is_cli_args = !ctx->opt.stream;
    bool new_backend = false;
    bool new_region = false;
    bool new_output = false;
    char * region_output = NULL;
    char * arg_output = NULL;

    while (argc > 0 && argv[0][0] == '-') {
        if (is_cli_args && (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "--help") == 0)) {
            usage_opt(ctx);
        } else if (strcmp(argv[0], "-v") == 0 || strcmp(argv[0], "--verbose") == 0) {
            ctx->opt.verbose = true;
        } else if (strcmp(argv[0], "--no-verbose") == 0) {
            ctx->opt.verbose = false;
        } else if (strcmp(argv[0], "-c") == 0 || strcmp(argv[0], "--show-cursor") == 0) {
            ctx->opt.show_cursor = true;
        } else if (strcmp(argv[0], "--no-show-cursor") == 0) {
            ctx->opt.show_cursor = false;
        } else if (strcmp(argv[0], "-n") == 0) {
            log_warn("options::parse(): -n is deprecated, use --no-show-cursor\n");
            ctx->opt.show_cursor = false;
        } else if (strcmp(argv[0], "-i") == 0 || strcmp(argv[0], "--invert-colors") == 0) {
            ctx->opt.invert_colors = true;
        } else if (strcmp(argv[0], "--no-invert-colors") == 0) {
            ctx->opt.invert_colors = false;
        } else if (strcmp(argv[0], "-s") == 0 || strcmp(argv[0], "--scaling") == 0) {
            if (argc < 2) {
                log_error("options::parse(): option %s requires an argument\n", argv[0]);
                if (is_cli_args) exit_fail(ctx);
            } else {
                if (!parse_scaling_opt(&ctx->opt.scaling, argv[1])) {
                    log_error("options::parse(): invalid scaling mode %s\n", argv[1]);
                    if (is_cli_args) exit_fail(ctx);
                }

                argv++;
                argc--;
            }
        } else if (strcmp(argv[0], "-b") == 0 || strcmp(argv[0], "--backend") == 0) {
            if (argc < 2) {
                log_error("options::parse(): option %s requires an argument\n", argv[0]);
                if (is_cli_args) exit_fail(ctx);
            } else {
                if (!parse_backend_opt(&ctx->opt.backend, argv[1])) {
                    log_error("options::parse(): invalid backend %s\n", argv[1]);
                    if (is_cli_args) exit_fail(ctx);
                }

                new_backend = true;
                argv++;
                argc--;
            }
        } else if (strcmp(argv[0], "-t") == 0 || strcmp(argv[0], "--transform") == 0) {
            if (argc < 2) {
                log_error("options::parse(): option %s requires an argument\n", argv[0]);
                if (is_cli_args) exit_fail(ctx);
            } else {
                if (!parse_transform_opt(&ctx->opt.transform, argv[1])) {
                    log_error("options::parse(): invalid transform %s\n", argv[1]);
                    if (is_cli_args) exit_fail(ctx);
                }

                argv++;
                argc--;
            }
        } else if (strcmp(argv[0], "-r") == 0 || strcmp(argv[0], "--region") == 0) {
            if (argc < 2) {
                log_error("options::parse(): option %s requires an argument\n", argv[0]);
                if (is_cli_args) exit_fail(ctx);
            } else {
                char * new_region_output = NULL;
                if (!parse_region_opt(&ctx->opt.region, &new_region_output, argv[1])) {
                    log_error("options::parse(): invalid region %s\n", argv[1]);
                    if (is_cli_args) exit_fail(ctx);
                } else {
                    ctx->opt.has_region = true;
                    free(region_output);
                    region_output = new_region_output;
                    new_region = true;
                }

                argv++;
                argc--;
            }
        } else if (strcmp(argv[0], "--no-region") == 0) {
            ctx->opt.has_region = false;
            ctx->opt.region = (region_t){ .x = 0, .y = 0, .width = 0, .height = 0 };
        } else if (strcmp(argv[0], "-S") == 0 || strcmp(argv[0], "--stream") == 0) {
            ctx->opt.stream = true;
        } else if (strcmp(argv[0], "--") == 0) {
            argv++;
            argc--;
            break;
        } else {
            log_error("options::parse(): invalid option %s\n", argv[0]);
            if (is_cli_args) exit_fail(ctx);
        }

        argv++;
        argc--;
    }

    if (argc > 0) {
        arg_output = strdup(argv[0]);
        if (arg_output == NULL) {
            log_error("options::parse(): failed to allocate copy of output name\n");
            if (is_cli_args) exit_fail(ctx);
        } else {
            new_output = true;
        }
    }

    if (new_output || new_region) {
        free(ctx->opt.output);
        ctx->opt.output = NULL;
    }

    if (new_output && !new_region) {
        ctx->opt.has_region = false;
        ctx->opt.region = (region_t){ .x = 0, .y = 0, .width = 0, .height = 0 };
    }

    if (!new_output && new_region && region_output == NULL) {
        // output implicitly defined by region
        ctx->opt.output = NULL;
    } else if (!new_output && new_region && region_output != NULL) {
        // output explicitly defined by region
        ctx->opt.output = region_output;
    } else if (new_output && new_region && region_output == NULL) {
        // output defined by argument
        // region must be in this output
        ctx->opt.output = arg_output;
    } else if (new_output && new_region && region_output != NULL) {
        // output defined by both region and argument
        // must be the same
        // region must be in this output
        if (strcmp(region_output, arg_output) != 0) {
            log_error("options::parse(): region and argument output differ: %s vs %s\n", region_output, arg_output);
            if (is_cli_args) exit_fail(ctx);
        }
        ctx->opt.output = region_output;
    } else if (new_output && !new_region) {
        // output defined by argument
        ctx->opt.output = arg_output;
    } else if (!new_output && !new_region && is_cli_args) {
        // no output or region specified
        usage_opt(ctx);
    }

    if (argc > 1) {
        log_error("options::parse(): unexpected trailing arguments after output name\n");
        if (is_cli_args) exit_fail(ctx);
    }

    output_list_node_t * target_output = NULL;
    region_t target_region = (region_t){ .x = 0, .y = 0, .width = 0, .height = 0 };
    if (!is_cli_args && find_output_opt(ctx, &target_output, &target_region)) {
        ctx->mirror.current_target = target_output;
        ctx->mirror.current_region = target_region;
    }

    if (!is_cli_args && new_backend) {
        init_mirror_backend(ctx);
    }

    if (!is_cli_args) {
        update_uniforms(ctx);
        update_title(ctx);
    }
}

