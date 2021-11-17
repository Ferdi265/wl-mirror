#ifndef WL_MIRROR_MIRROR_H_
#define WL_MIRROR_MIRROR_H_

#include <stdint.h>
#include <stdbool.h>
#include <wayland-client-protocol.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include "wlr-export-dmabuf-unstable-v1.h"

struct ctx;
struct output_list_node;

typedef struct {
    int32_t fd;
    uint32_t size;
    uint32_t offset;
    uint32_t stride;
    uint32_t plane_index;
} dmabuf_object_t;

typedef enum {
    STATE_WAIT_FRAME,
    STATE_WAIT_OBJECTS,
    STATE_WAIT_READY,
    STATE_READY,
    STATE_CANCELED
} dmabuf_state_t;

typedef struct ctx_mirror {
    struct output_list_node * current;
    struct wl_callback * frame_callback;
    struct zwlr_export_dmabuf_frame_v1 * frame;
    bool invert_y;

    // gl data
    EGLImage frame_image;

    // frame data
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    uint32_t buffer_flags;
    uint32_t frame_flags;
    uint32_t format;
    uint32_t modifier_low;
    uint32_t modifier_high;
    uint32_t num_objects;

    // object data
    dmabuf_object_t objects[4];

    // state flags
    dmabuf_state_t state;
    uint32_t processed_objects;
} ctx_mirror_t;

void init_mirror(struct ctx * ctx);

void output_removed_mirror(struct ctx * ctx, struct output_list_node * node);

void cleanup_mirror(struct ctx * ctx);

#endif
