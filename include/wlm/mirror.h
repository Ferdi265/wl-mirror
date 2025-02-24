#ifndef WL_MIRROR_MIRROR_H_
#define WL_MIRROR_MIRROR_H_

#include <stdint.h>
#include <stdbool.h>
#include <wayland-client-protocol.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <wlm/transform.h>
#include <wlm/mirror-backends.h>

struct ctx;
struct output_list_node;

typedef struct fallback_backend fallback_backend_t;
struct fallback_backend {
    char * name;
    void (*init)(struct ctx * ctx);
};

typedef struct ctx_mirror {
    struct output_list_node * current_target;
    struct wl_callback * frame_callback;
    region_t current_region;
    bool invert_y;

    // backend data
    mirror_backend_t * backend;
    fallback_backend_t * fallback_backends;
    size_t auto_backend_index;

    // state flags
    bool initialized;
} ctx_mirror_t;

void wlm_mirror_init(struct ctx * ctx);
void wlm_mirror_backend_init(struct ctx * ctx);

void wlm_mirror_output_removed(struct ctx * ctx, struct output_list_node * node);
void wlm_mirror_update_title(struct ctx * ctx);
void wlm_mirror_options_updated(struct ctx * ctx);

void wlm_mirror_backend_fail(struct ctx * ctx);
void wlm_mirror_cleanup(struct ctx * ctx);

#endif
