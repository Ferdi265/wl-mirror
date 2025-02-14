#include <stdlib.h>
#include <wlm/context.h>

#define WLM_LOG_COMPONENT options

static void opt_spec_length(const wlm_opt_spec_t * spec, int * short_length, int * long_length) {
    int arg_length   = spec->arg_name == NULL ? 0 : 1 + strlen(spec->arg_name);
    *short_length = spec->short_name == NULL ? 0 : arg_length + strlen(spec->short_name) + 2;
    *long_length  = spec->long_name == NULL ? 0 : arg_length + strlen(spec->long_name);
}

static void opt_spec_print(ctx_t * ctx, const wlm_opt_spec_t * spec, int max_short_length, int max_long_length) {
    int short_length, long_length;
    opt_spec_length(spec, &short_length, &long_length);

    int short_pad = max_short_length - short_length;
    int long_pad  = max_long_length - long_length;

    wlm_log(ctx, WLM_TRACE, "%-20s: pad: short = %d, long = %d", spec->long_name, short_pad, long_pad);
    wlm_print(ctx, WLM_FATAL, "  %s%s%s%s%-*s%s%s%s%s%-*s%s",
        spec->short_name == NULL ? "" : spec->short_name,
        spec->short_name == NULL || spec->arg_name == NULL ? "" : " ",
        spec->short_name == NULL || spec->arg_name == NULL ? "" : spec->arg_name,
        spec->short_name == NULL ? "" : ", ",
        short_pad,
        "",
        spec->long_name == NULL ? "" : spec->long_name,
        spec->long_name == NULL || spec->arg_name == NULL ? "" : " ",
        spec->long_name == NULL || spec->arg_name == NULL ? "" : spec->arg_name,
        spec->long_name == NULL ? "" : " ",
        long_pad,
        "",
        spec->inline_help
    );
}

void wlm_opt_print_help(ctx_t * ctx) {
    wlm_print(ctx, WLM_FATAL, "usage: wl-mirror [options] [output]");

    // count lengths
    int max_short_length = 0;
    int max_long_length = 0;
    const wlm_opt_spec_t * spec = wlm_opt_specs;
    for (; spec->on_parse != NULL; spec++) {
        int short_length, long_length;
        opt_spec_length(spec, &short_length, &long_length);
        wlm_log(ctx, WLM_DEBUG, "%-20s: short = %d, long = %d", spec->long_name, short_length, long_length);

        if (short_length > max_short_length) max_short_length = short_length;
        if (long_length > max_long_length) max_long_length = long_length;
    }

    wlm_log(ctx, WLM_DEBUG, "max short length: %d", max_short_length);
    wlm_log(ctx, WLM_DEBUG, "max long  length: %d", max_long_length);

    // print CLI inline help
    wlm_print(ctx, WLM_FATAL, "");
    wlm_print(ctx, WLM_FATAL, "cli options:");
    spec = wlm_opt_specs;
    for (; spec->on_parse != NULL; spec++) {
        if (!(spec->allow_mode & WLM_OPT_CLI)) continue;
        opt_spec_print(ctx, spec, max_short_length, max_long_length);
    }

    // print STREAM inline help
    wlm_print(ctx, WLM_FATAL, "");
    wlm_print(ctx, WLM_FATAL, "stream options:");
    spec = wlm_opt_specs;
    for (; spec->on_parse != NULL; spec++) {
        if (!(spec->allow_mode & WLM_OPT_STREAM)) continue;
        opt_spec_print(ctx, spec, max_short_length, max_long_length);
    }

    // print LONG inline help
    spec = wlm_opt_specs;
    for (; spec->on_parse != NULL; spec++) {
        if (spec->long_help == NULL) continue;
        wlm_print(ctx, WLM_FATAL, "\n%s", spec->long_help);
    }

    wlm_cleanup(ctx);
    exit(0);
}

void wlm_opt_parse(ctx_t * ctx, int argc, char ** argv) {
    bool has_error = false;
    while (argc > 0 && argv[0][0] == '-') {
        const char * opt = argv[0];
        argc--, argv++;

        if (strcmp(opt, "--") == 0) break;

        bool match = false;
        const wlm_opt_spec_t * spec = wlm_opt_specs;
        for (; spec->on_parse != NULL; spec++) {
            match = match || (spec->short_name != NULL && strcmp(spec->short_name, opt) == 0);
            match = match || (spec->long_name != NULL && strcmp(spec->long_name, opt) == 0);
            if (match) break;
        }

        if (!match) {
            wlm_log(ctx, WLM_ERROR, "invalid option '%s'", opt);
            has_error = true;
            continue;
        }

        if (spec->arg_name != NULL && argc == 0) {
            wlm_log(ctx, WLM_ERROR, "option '%s' requires an argument", opt);
            has_error = true;
            continue;
        }

        const char * arg = argv[0];
        if (!spec->on_parse(ctx, arg)) {
            has_error = true;
            continue;
        }
    }

    if (has_error) {
        wlm_log(ctx, WLM_FATAL, "invalid usage, see 'wl-mirror --help'");
        wlm_exit_fail(ctx);
    }
}

void wlm_opt_zero(ctx_t * ctx) {
    (void)ctx;
}

void wlm_opt_cleanup(ctx_t * ctx) {
    (void)ctx;
}
