#include <wayland-client-protocol.h>
#include <wlm/context.h>
#include <wlm/wayland.h>
#include <wlm/mirror/target.h>
#include <wlm/proto/ext-image-capture-source-v1.h>
#include <stdlib.h>
#include <string.h>

// --- static helper functions

static wlm_mirror_target_t * create_null_target(ctx_t * ctx) {
    wlm_mirror_target_t * target = calloc(1, sizeof *target);
    target->type = WLM_MIRROR_TARGET_TYPE_NULL;
    target->source = NULL;

    (void)ctx;
    return target;
}

static wlm_mirror_target_t * create_output_target(ctx_t * ctx, output_list_node_t * output_node) {
    wlm_mirror_target_output_t * output_target = calloc(1, sizeof *output_target);
    output_target->header.type = WLM_MIRROR_TARGET_TYPE_OUTPUT;
    output_target->header.source = NULL;
    output_target->header.transform = output_node->transform;
    output_target->output = output_node;

    if (ctx->wl.output_capture_source_manager != NULL) {
        output_target->header.source = ext_output_image_capture_source_manager_v1_create_source(ctx->wl.output_capture_source_manager, output_node->output);
    }

    return (wlm_mirror_target_t *)output_target;
}

static wlm_mirror_target_t * create_toplevel_target(ctx_t * ctx, struct ext_foreign_toplevel_handle_v1 * toplevel) {
    wlm_mirror_target_toplevel_t * toplevel_target = calloc(1, sizeof *toplevel_target);
    toplevel_target->header.type = WLM_MIRROR_TARGET_TYPE_OUTPUT;
    toplevel_target->header.source = NULL;
    toplevel_target->header.transform = WL_OUTPUT_TRANSFORM_NORMAL;
    toplevel_target->toplevel = toplevel;

    if (ctx->wl.toplevel_capture_source_manager != NULL) {
        toplevel_target->header.source = ext_foreign_toplevel_image_capture_source_manager_v1_create_source(ctx->wl.toplevel_capture_source_manager, toplevel);
    }

    return (wlm_mirror_target_t *)toplevel_target;
}

// --- public functions ---

wlm_mirror_target_t * wlm_mirror_target_parse(ctx_t * ctx, const char * target_str) {
    // find prefix
    const char * prefix_separator = strchr(target_str, ':');

    // no prefix
    if (prefix_separator == NULL) {
        // match output using the whole target string
        // fallback to legacy wl-mirror behaviour
        return wlm_mirror_target_find_output(ctx, target_str);
    }

    // find prefix length and pointer to target name after prefix
    int prefix_len = prefix_separator - target_str;
    const char * prefix_str = target_str;
    const char * target_name_str = prefix_separator + 1;

    // match different prefix kinds
    if (strncmp(prefix_str, "null:", prefix_len) == 0 && strcmp(target_name_str, "") == 0) {
        return create_null_target(ctx);
    } else if (strncmp(prefix_str, "output:", prefix_len) == 0) {
        // match output
        return wlm_mirror_target_find_output(ctx, target_name_str);
    } else if (strncmp(prefix_str, "toplevel:", prefix_len) == 0) {
        // match toplevel
        return wlm_mirror_target_find_toplevel(ctx, target_name_str);
    } else {
        // unknown prefix
        wlm_log_error("mirror-target::parse(): invalid target prefix '%.*s'\n", prefix_len, prefix_str);
        return NULL;
    }
}

// TODO: remove this
wlm_mirror_target_t * wlm_mirror_target_create_output(ctx_t * ctx, output_list_node_t * output_node) {
    return create_output_target(ctx, output_node);
}

wlm_mirror_target_t * wlm_mirror_target_find_output(ctx_t * ctx, const char * name) {
    // try to match output by name
    output_list_node_t * output_node = NULL;
    if (!wlm_wayland_find_output(ctx, name, &output_node)) {
        return NULL;
    }

    // kanshi syntax:
    //   match when:
    //     name == '*' or
    //     name == output name or
    //     name == '{output make} {output model} {output serial}'
    //
    // sway syntax
    //   match when
    //     name == '*' or
    //     name == output name or
    //     name == '{output make} {output model} {output serial}'
    //
    // wl-mirror syntax
    //   match when
    //     name == output name or
    //     name == '{output make} {output model} {output serial}' or

    // TODO: double-check that this syntax is ok

    return create_output_target(ctx, output_node);
}

wlm_mirror_target_t * wlm_mirror_target_find_toplevel(ctx_t * ctx, const char * identifier) {
    wlm_log_warn("mirror-target::find_toplevel(): finding target toplevel not yet implemented\n");

    return NULL;

    // wl-mirror syntax
    //   match when
    //     identifier == toplevel identifier or
    //     identifier == '{toplevel app_id}' or
    //     identifier == '{toplevel app_id} {toplevel title}'

    (void)ctx;
    (void)identifier;
    (void)create_toplevel_target;
}

output_list_node_t * wlm_mirror_target_get_output_node(wlm_mirror_target_t * target) {
    if (target == NULL) {
        return NULL;
    }

    if (target->type != WLM_MIRROR_TARGET_TYPE_OUTPUT) {
        return NULL;
    }

    wlm_mirror_target_output_t * output_target = (wlm_mirror_target_output_t *)target;
    return output_target->output;
}

struct ext_image_capture_source_v1 * wlm_mirror_target_get_capture_source(wlm_mirror_target_t * target) {
    if (target == NULL) {
        return NULL;
    }

    return target->source;
}

enum wl_output_transform wlm_mirror_target_get_transform(wlm_mirror_target_t * target) {
    if (target == NULL) {
        return WL_OUTPUT_TRANSFORM_NORMAL;
    }

    return target->transform;
}

void wlm_mirror_target_destroy(wlm_mirror_target_t * target) {
    if (target == NULL) {
        return;
    }

    if (target->source != NULL) {
        ext_image_capture_source_v1_destroy(target->source);
    }

    free(target);
}
