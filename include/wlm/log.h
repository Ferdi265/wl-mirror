#ifndef WL_MIRROR_LOG_H_
#define WL_MIRROR_LOG_H_

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <wlm/util.h>

typedef struct ctx ctx_t;

typedef enum {
    WLM_FATAL = 0,
    WLM_ERROR = 1,
    WLM_WARN = 2,
    WLM_INFO = 3,
    WLM_DEBUG = 4,
    WLM_TRACE = 5
} wlm_log_level_t;

#define WLM_PRINT_LOG_LEVEL(level) \
    level == WLM_FATAL ? "FATAL" : \
    level == WLM_ERROR ? "ERROR" : \
    level == WLM_WARN  ? "WARN" : \
    level == WLM_INFO  ? "INFO" : \
    level == WLM_DEBUG ? "DEBUG" : \
    level == WLM_TRACE ? "TRACE" : \
    "???"

typedef struct {
    const char * name;
    wlm_log_level_t level;
} wlm_log_level_spec_t;

extern const wlm_log_level_spec_t wlm_log_level_specs[];

#define WLM_LOG_LEVEL_SPEC(log_level) \
    (wlm_log_level_spec_t){ \
        .name = WLM_PRINT_LOG_LEVEL(log_level), \
        .level = log_level \
    }

#define WLM_LOG_LEVEL_SPEC_END \
    (wlm_log_level_spec_t){ \
        .name = NULL, \
    }

typedef struct {
    const char * name;
    size_t level_offset;
} wlm_log_component_spec_t;

extern const wlm_log_component_spec_t wlm_log_component_specs[];

#define WLM_LOG_COMPONENT_SPEC(component_name) \
    (wlm_log_component_spec_t){ \
        .name = WLM_STRINGIFY(component_name), \
        .level_offset = offsetof(ctx_t, log. WLM_CONCAT(component_name, _level)) \
    }

#define WLM_LOG_COMPONENT_SPEC_END \
    (wlm_log_component_spec_t){ \
        .name = NULL, \
    }

typedef struct {
    wlm_log_level_t all_level;
    wlm_log_level_t main_level;
    wlm_log_level_t log_level;
    wlm_log_level_t event_level;
    wlm_log_level_t wayland_level;
    wlm_log_level_t egl_level;
} ctx_log_t;

#define wlm_log(ctx, level, fmt, ...) \
    do { \
        if ((ctx)->log. WLM_CONCAT(WLM_LOG_COMPONENT, _level) >= (level)) { \
            fprintf(stderr, "wl-mirror | %-5s | %-7s | %s() in %s:%-3d || " fmt "\n", \
                WLM_PRINT_LOG_LEVEL(level), \
                WLM_STRINGIFY(WLM_LOG_COMPONENT), \
                __func__, __FILE__, __LINE__, \
                ##__VA_ARGS__ \
            ); \
        } \
    } while (0)

void wlm_log_zero(ctx_t *);
void wlm_log_init(ctx_t *);
void wlm_log_cleanup(ctx_t *);

#endif
