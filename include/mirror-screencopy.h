#ifndef WL_MIRROR_MIRROR_SCREENCOPY_H_
#define WL_MIRROR_MIRROR_SCREENCOPY_H_

#include <stdint.h>
#include "mirror.h"
#include "wlr-screencopy-unstable-v1.h"
#include <wayland-client.h>

typedef enum {
    STATE_WAIT_FRAME,
    STATE_WAIT_READY,
    STATE_READY,
    STATE_CANCELED
} screencopy_state_t;

typedef struct {
    mirror_backend_t header;

    // shm state
    int shm_fd;
    size_t shm_size;

    // wl_shm objects
    struct wl_shm_pool * shm_pool;
    struct wl_buffer * shm_buffer;

    // screencopy objects
    struct zwlr_screencopy_frame_v1 * screencopy_frame;

    // screencopy state flags
    screencopy_state_t state;
} screencopy_mirror_backend_t;

#endif
