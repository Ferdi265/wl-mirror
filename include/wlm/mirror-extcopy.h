#ifndef WL_MIRROR_MIRROR_EXTCOPY_H_
#define WL_MIRROR_MIRROR_EXTCOPY_H_

#include <wlm/mirror.h>
#include <wlm/proto/ext-image-copy-capture-v1.h>
#include <wlm/proto/ext-image-capture-source-v1.h>

#ifdef WITH_GBM
#include <gbm.h>
#endif

typedef enum {
    STATE_INIT,
    STATE_WAIT_BUFFER_INFO,
    STATE_READY,
    STATE_WAIT_READY,
    STATE_CANCELED
    // TODO
} extcopy_state_t;

typedef struct {
    mirror_backend_t header;

    struct ext_image_capture_source_v1 * capture_source;
    struct ext_image_copy_capture_session_v1 * capture_session;
    struct ext_image_copy_capture_frame_v1 * capture_frame;

#ifdef WITH_GBM
    struct gbm_device * gbm_device;
    struct gbm_bo * gbm_dmabuf;
#endif

    extcopy_state_t state;
} extcopy_mirror_backend_t;

#endif
