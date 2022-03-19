#ifndef WL_MIRROR_MIRROR_H_
#define WL_MIRROR_MIRROR_H_

#include <stdint.h>
#include <stdbool.h>
#include <wayland-client-protocol.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include "transform.h"

struct ctx;
struct output_list_node;

#define MIRROR_AUTO_FALLBACK_FAILCOUNT 10

typedef struct mirror_backend {
    void (*on_frame)(struct ctx * ctx);
    void (*on_cleanup)(struct ctx * ctx);
    size_t fail_count;
} mirror_backend_t;

typedef struct ctx_mirror {
    struct output_list_node * current_target;
    struct wl_callback * frame_callback;
    region_t current_region;
    bool invert_y;

    // backend data
    mirror_backend_t * backend;
    size_t auto_backend_index;

    // gl data
    EGLImage frame_image;

    // state flags
    bool initialized;
} ctx_mirror_t;

void init_mirror(struct ctx * ctx);
void init_mirror_backend(struct ctx * ctx);

void output_removed_mirror(struct ctx * ctx, struct output_list_node * node);
void update_options_mirror(struct ctx * ctx);

void cleanup_mirror(struct ctx * ctx);

#endif
