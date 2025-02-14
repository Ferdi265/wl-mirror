#include <wlm/context.h>

#define WLM_LOG_COMPONENT options

static bool on_help_parse(ctx_t * ctx, const char * arg) {
    (void)arg;

    wlm_opt_print_help(ctx);
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
