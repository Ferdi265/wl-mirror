#ifndef WL_MIRROR_OPTIONS_H_
#define WL_MIRROR_OPTIONS_H_

#include <stdbool.h>
#include <stdnoreturn.h>

typedef struct ctx ctx_t;

typedef enum {
    WLM_OPT_CLI = 1 << 0,
    WLM_OPT_STREAM = 1 << 1,
    WLM_OPT_ALWAYS = WLM_OPT_CLI | WLM_OPT_STREAM
} wlm_opt_mode_t;

typedef struct {
    const char * short_name;
    const char * long_name;
    const char * arg_name;
    const char * inline_help;
    const char * long_help;
    wlm_opt_mode_t allow_mode;

    bool (* on_parse)(ctx_t *, const char * arg);
} wlm_opt_spec_t;

extern const wlm_opt_spec_t wlm_opt_specs[];

#define WLM_OPT_SPEC_FLAG(_short, _long, allow, help, parse) \
    (wlm_opt_spec_t){ \
        .short_name = _short, \
        .long_name = _long, \
        .arg_name = NULL, \
        .inline_help = help, \
        .long_help = NULL, \
        .allow_mode = allow, \
        \
        .on_parse = parse, \
    }

#define WLM_OPT_SPEC_ARG(_short, _long, _arg, allow, help, parse) \
    (wlm_opt_spec_t){ \
        .short_name = _short, \
        .long_name = _long, \
        .arg_name = _arg, \
        .inline_help = help, \
        .long_help = NULL, \
        .allow_mode = allow, \
        \
        .on_parse = parse, \
    }

#define WLM_OPT_SPEC_LONG(_short, _long, _arg, allow, help, _long_help, parse) \
    (wlm_opt_spec_t){ \
        .short_name = _short, \
        .long_name = _long, \
        .arg_name = _arg, \
        .inline_help = help, \
        .long_help = _long_help, \
        .allow_mode = allow, \
        \
        .on_parse = parse, \
    }

#define WLM_OPT_SPEC_END (wlm_opt_spec_t){ .on_parse = NULL }

typedef struct {

} ctx_opt_t;

void wlm_opt_zero(ctx_t *);
void wlm_opt_cleanup(ctx_t *);

void wlm_opt_parse(ctx_t *, int argc, char ** argv);
noreturn void wlm_opt_print_help(ctx_t *);

#endif
