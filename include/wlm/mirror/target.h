#ifndef WLM_MIRROR_TARGET_H_
#define WLM_MIRROR_TARGET_H_

#include <wayland-client-protocol.h>
#include <wlm/wayland.h>

typedef struct ctx ctx_t;

typedef enum {
    WLM_MIRROR_TARGET_TYPE_NULL = 0,
    WLM_MIRROR_TARGET_TYPE_OUTPUT,
    WLM_MIRROR_TARGET_TYPE_TOPLEVEL,
} wlm_mirror_target_type_t;

typedef struct {
    wlm_mirror_target_type_t type;
    struct ext_image_capture_source_v1 * source;
    enum wl_output_transform transform;
} wlm_mirror_target_t;

typedef struct {
    wlm_mirror_target_t header;
    output_list_node_t * output;
} wlm_mirror_target_output_t;

typedef struct {
    wlm_mirror_target_t header;
    struct ext_foreign_toplevel_handle_v1 * toplevel;
} wlm_mirror_target_toplevel_t;

wlm_mirror_target_t * wlm_mirror_target_parse(ctx_t * ctx, const char * target);
wlm_mirror_target_t * wlm_mirror_target_find_output(ctx_t * ctx, const char * name);
wlm_mirror_target_t * wlm_mirror_target_find_toplevel(ctx_t * ctx, const char * identifier);

// TODO: remove this
wlm_mirror_target_t * wlm_mirror_target_create_output(ctx_t * ctx, output_list_node_t * output_node);

output_list_node_t * wlm_mirror_target_get_output_node(wlm_mirror_target_t * target);
struct ext_image_capture_source_v1 * wlm_mirror_target_get_capture_source(wlm_mirror_target_t * target);
enum wl_output_transform wlm_mirror_target_get_transform(wlm_mirror_target_t * target);

void wlm_mirror_target_destroy(wlm_mirror_target_t * target);

#endif
