#ifndef WL_MIRROR_MIRROR_EXTCOPY_H_
#define WL_MIRROR_MIRROR_EXTCOPY_H_

#include <wlm/mirror.h>
#include <wlm/wayland.h>
#include <wlm/proto/ext-image-copy-capture-v1.h>
#include <wlm/proto/ext-image-capture-source-v1.h>
#include <wlm/proto/color-management-v1.h>
#include <wlm/proto/ext-image-capture-color-management-v1.h>

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
    bool use_dmabuf;

    struct ext_image_capture_source_v1 * capture_source;
    struct ext_image_copy_capture_session_v1 * capture_session;
    struct ext_image_copy_capture_frame_v1 * capture_frame;
	struct ext_image_capture_source_colors_v1 * capture_source_colors;

	struct wp_image_description_v1 * pending_desc;
	struct wp_image_description_v1 * ready_desc;
	struct wp_image_description_v1 * frame_desc;
	bool frame_desc_is_surface_desc;

    bool has_shm_format;
    bool has_drm_format;
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t frame_shm_stride;
    uint32_t frame_shm_format;
    uint32_t frame_drm_format;
    uint64_t * frame_drm_modifiers;
    size_t frame_num_drm_modifiers;

    extcopy_state_t state;
} extcopy_mirror_backend_t;

#endif
