#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlm/context.h>

#define WLM_LOG_COMPONENT log

const wlm_log_level_spec_t wlm_log_level_specs[] = {
    WLM_LOG_LEVEL_SPEC(WLM_FATAL),
    WLM_LOG_LEVEL_SPEC(WLM_ERROR),
    WLM_LOG_LEVEL_SPEC(WLM_WARN),
    WLM_LOG_LEVEL_SPEC(WLM_INFO),
    WLM_LOG_LEVEL_SPEC(WLM_DEBUG),
    WLM_LOG_LEVEL_SPEC(WLM_TRACE),
    WLM_LOG_LEVEL_SPEC_END
};

const wlm_log_component_spec_t wlm_log_component_specs[] = {
    WLM_LOG_COMPONENT_SPEC(all),
    WLM_LOG_COMPONENT_SPEC(main),
    WLM_LOG_COMPONENT_SPEC(log),
    WLM_LOG_COMPONENT_SPEC(event),
    WLM_LOG_COMPONENT_SPEC(wayland),
    WLM_LOG_COMPONENT_SPEC(egl),
    WLM_LOG_COMPONENT_SPEC_END
};

static void set_log_level_all(ctx_t * ctx, wlm_log_level_t level) {
    const wlm_log_component_spec_t * spec = wlm_log_component_specs;
    for (; spec->name != NULL; spec++) {
        *(wlm_log_level_t *)((char *)ctx + spec->level_offset) = level;
    }
}

static bool parse_log_component(ctx_t * ctx, const char * component_str, size_t length, wlm_log_level_t ** level_ptr) {
    const wlm_log_component_spec_t * spec = wlm_log_component_specs;
    for (; spec->name != NULL; spec++) {
        if (strlen(spec->name) >= length && strncmp(spec->name, component_str, length) == 0) {
            *level_ptr = (wlm_log_level_t *)((char *)ctx + spec->level_offset);
            return true;
        }
    }

    wlm_log(ctx, WLM_ERROR, "invalid log component '%.*s'", (int)length, component_str);
    return false;
}

static bool parse_log_level(ctx_t * ctx, const char * level_str, size_t length, wlm_log_level_t * level) {
    const wlm_log_level_spec_t * spec = wlm_log_level_specs;
    for (; spec->name != NULL; spec++) {
        if (strlen(spec->name) >= length && strncmp(spec->name, level_str, length) == 0) {
            *level = spec->level;
            return true;
        }
    }

    wlm_log(ctx, WLM_ERROR, "invalid log level '%.*s'", (int)length, level_str);
    return false;
}

static bool parse_log_level_entry(ctx_t * ctx, const char * entry, size_t length) {
    size_t name_length = strcspn(entry, "=");
    if (name_length > length) name_length = length;

    // value starts one character after name
    const char * name = entry;
    const char * value = entry + name_length + 1;
    size_t value_length = length - name_length - 1;

    if (name_length == length) {
        // no '=' found in entry
        // assume name is 'all' and entry is only value
        name = "all";
        name_length = strlen(name);
        value = entry;
        value_length = length;
    }

    wlm_log_level_t * level = NULL;
    if (!parse_log_component(ctx, name, name_length, &level)) {
        return false;
    }

    if (!parse_log_level(ctx, value, value_length, level)) {
        return false;
    }

    if (level == &ctx->log.all_level) {
        wlm_log(ctx, WLM_DEBUG, "setting all components to log level '%s'", WLM_PRINT_LOG_LEVEL(*level));
        set_log_level_all(ctx, *level);
    } else {
        wlm_log(ctx, WLM_DEBUG, "setting component '%.*s' to log level '%s'", (int)name_length, name, WLM_PRINT_LOG_LEVEL(*level));
    }

    return true;
}

static void parse_log_level_config(ctx_t * ctx, const char * config) {
    const char * config_ptr = config;

    while (*config_ptr != '\0') {
        const char * entry = config_ptr;
        size_t entry_length = strcspn(entry, ":");

        config_ptr += entry_length;
        if (*config_ptr == ':') config_ptr += 1;

        parse_log_level_entry(ctx, entry, entry_length);
    }
}

// initialization

void wlm_log_zero(ctx_t * ctx) {
    set_log_level_all(ctx, WLM_WARN);
}

void wlm_log_init(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "initializing");

    const char * log_level_env = getenv("WLM_LOG_LEVEL");
    if (log_level_env != NULL) {
        parse_log_level_config(ctx, log_level_env);
    }
}

void wlm_log_cleanup(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "cleaning up");

    // nothing to clean up

    wlm_log_zero(ctx);
}
