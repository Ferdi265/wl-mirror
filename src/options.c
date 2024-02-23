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

static bool on_help_parse(ctx_t * ctx, const char * arg) {
    (void)arg;

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

static bool on_option_parse_TODO(ctx_t * ctx, const char * arg) {
    return true;
}

#define on_version_parse on_option_parse_TODO
#define on_verbose_parse on_option_parse_TODO
#define on_no_verbose_parse on_option_parse_TODO
#define on_log_level_parse on_option_parse_TODO
#define on_show_cursor_parse on_option_parse_TODO
#define on_no_show_cursor_parse on_option_parse_TODO
#define on_invert_colors_parse on_option_parse_TODO
#define on_no_invert_colors_parse on_option_parse_TODO
#define on_freeze_parse on_option_parse_TODO
#define on_unfreeze_parse on_option_parse_TODO
#define on_toggle_freeze_parse on_option_parse_TODO
#define on_fullscreen_parse on_option_parse_TODO
#define on_fullscreen_output_parse on_option_parse_TODO
#define on_unfullscreen_parse on_option_parse_TODO
#define on_toggle_fullscreen_parse on_option_parse_TODO
#define on_scaling_parse on_option_parse_TODO

const wlm_opt_spec_t wlm_opt_specs[] = {
    WLM_OPT_SPEC_FLAG("-h", "--help", WLM_OPT_CLI,
        "show this help",
        on_help_parse
    ),
    WLM_OPT_SPEC_FLAG("-V", "--version", WLM_OPT_CLI,
        "print version",
        on_version_parse
    ),
    WLM_OPT_SPEC_FLAG("-v", "--verbose", WLM_OPT_ALWAYS,
        "increase log level to DEBUG",
        on_verbose_parse
    ),
    WLM_OPT_SPEC_FLAG(NULL, "--no-verbose", WLM_OPT_ALWAYS,
        "decrease log level to WARN",
        on_no_verbose_parse
    ),
    WLM_OPT_SPEC_LONG("-l", "--log-level", "L", WLM_OPT_ALWAYS,
        "set log level to L (see 'log level' below)",
        (
            "log level:\n"
            "  log level can be set for the whole application or for individual components\n"
            "  the syntax is a colon-separated list of log levels for the components\n"
            "  - e.g. 'INFO', or 'INFO:wayland=DEBUG', or 'INFO:wayland=DEBUG:egl=TRACE'\n"
            "\n"
            "  the following log levels are supported:\n"
            "  - FATAL     only print fatal errors\n"
            "  - ERROR     also print errors\n"
            "  - WARN      also print warnings (default)\n"
            "  - INFO      also print informational messages\n"
            "  - DEBUG     also print debug messages\n"
            "  - TRACE     also print low-level trace messages\n"
            "\n"
            "  the following components exist:\n"
            "  - main      main function and application lifecycle\n"
            "  - log       logging system\n"
            "  - options   option parsing\n"
            "  - event     event system\n"
            "  - wayland   wayland protocol\n"
            "  - egl       EGL rendering and graphics handling"
        ),
        on_log_level_parse
    ),
    WLM_OPT_SPEC_FLAG("-c", "--show-cursor", WLM_OPT_ALWAYS,
        "show the cursor on the mirrored screen (default)",
        on_show_cursor_parse
    ),
    WLM_OPT_SPEC_FLAG(NULL, "--no-show-cursor", WLM_OPT_ALWAYS,
        "don't show the cursor on the mirrored screen",
        on_no_show_cursor_parse
    ),
    WLM_OPT_SPEC_FLAG("-i", "--invert-colors", WLM_OPT_ALWAYS,
        "invert colors in the mirrored screen",
        on_invert_colors_parse
    ),
    WLM_OPT_SPEC_FLAG(NULL, "--no-invert-colors", WLM_OPT_ALWAYS,
        "don't invert colors in the mirrored screen (default)",
        on_no_invert_colors_parse
    ),
    WLM_OPT_SPEC_FLAG("-f", "--freeze", WLM_OPT_STREAM,
        "freeze the current image on the screen",
        on_freeze_parse
    ),
    WLM_OPT_SPEC_FLAG(NULL, "--unfreeze", WLM_OPT_STREAM,
        "unfreeze the current image on the screen",
        on_unfreeze_parse
    ),
    WLM_OPT_SPEC_FLAG(NULL, "--toggle-freeze", WLM_OPT_STREAM,
        "toggle freeze state of screen capture",
        on_toggle_freeze_parse
    ),
    WLM_OPT_SPEC_FLAG("-F", "--fullscreen", WLM_OPT_ALWAYS,
        "fullscreen the wl-mirror window",
        on_fullscreen_parse
    ),
    WLM_OPT_SPEC_ARG(NULL, "--fullscreen-output", "O", WLM_OPT_ALWAYS,
        "fullscreen the wl-mirror window on a specific output",
        on_fullscreen_output_parse
    ),
    WLM_OPT_SPEC_FLAG(NULL, "--unfullscreen", WLM_OPT_STREAM,
        "unfullscreen the wl-mirror window",
        on_unfullscreen_parse
    ),
    WLM_OPT_SPEC_FLAG(NULL, "--toggle-fullscreen", WLM_OPT_STREAM,
        "toggle the fullscreen state of the wl-mirror window",
        on_toggle_fullscreen_parse
    ),
    WLM_OPT_SPEC_LONG("-s", "--scaling", "S", WLM_OPT_ALWAYS,
        "set the scaling mode for wl-mirror",
        (
            "scaling:\n"
            "  the scaling setting consists of a scaling mode and a filter\n"
            "  passing the option multiple times allows setting both\n"
            "  - e.g. '-s nearest', or '-s exact -s nearest'\n"
            "\n"
            "  scaling modes:\n"
            "  - fit      scale to fit (default)\n"
            "  - cover    scale to cover, cropping\n"
            "  - exact    scale to exact multiples\n"
            "\n"
            "  scaling filters:\n"
            "  - linear   linear scaling (default)\n"
            "  - nearest  nearest neighbor scaling"
        ),
        on_scaling_parse
    ),
    WLM_OPT_SPEC_END
};

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
