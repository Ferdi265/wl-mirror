#ifndef WL_MIRROR_MIRROR_SCREENCOPY_H_
#define WL_MIRROR_MIRROR_SCREENCOPY_H_

#include <stdint.h>
#include <wlm/mirror.h>
#include <wlm/proto/wlr-screencopy-unstable-v1.h>
#include <wayland-client.h>

typedef enum {
    STATE_WAIT_BUFFER,
    STATE_WAIT_BUFFER_DONE,
    STATE_WAIT_FLAGS,
    STATE_WAIT_READY,
    STATE_READY,
    STATE_CANCELED
} screencopy_state_t;

typedef struct {
    mirror_backend_t header;

    // screencopy frame object
    struct zwlr_screencopy_frame_v1 * screencopy_frame;

    // frame data
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t frame_stride;
    uint32_t frame_format;
    uint32_t frame_flags;

    // screencopy state flags
    screencopy_state_t state;
} screencopy_mirror_backend_t;

#endif
