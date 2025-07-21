#ifndef WLM_WAYLAND_PROTOCOLS_H_
#define WLM_WAYLAND_PROTOCOLS_H_

#include <wayland-client-protocol.h>
#include <wlm/proto/viewporter.h>
#include <wlm/proto/fractional-scale-v1.h>
#include <wlm/proto/xdg-shell.h>
#include <wlm/proto/xdg-output-unstable-v1.h>
#include <wlm/proto/wlr-export-dmabuf-unstable-v1.h>
#include <wlm/proto/wlr-screencopy-unstable-v1.h>
#include <wlm/proto/ext-image-copy-capture-v1.h>
#include <wlm/proto/ext-image-capture-source-v1.h>
#include <wlm/proto/ext-foreign-toplevel-list-v1.h>
#include <wlm/proto/linux-dmabuf-unstable-v1.h>

typedef struct ctx_wl_protocols {
    // required
    struct wl_compositor *                      compositor;
    struct wp_viewporter *                      viewporter;
    struct xdg_wm_base *                        xdg_wm_base;
    struct zxdg_output_manager_v1 *             xdg_output_manager;

    // optional
    struct wl_shm *                             shm;
    struct zwp_linux_dmabuf_v1 *                linux_dmabuf;
    struct wp_fractional_scale_manager_v1 *     fractional_scale_manager;

    // export-dmabuf backend
    struct zwlr_export_dmabuf_manager_v1 *      export_dmabuf_manager;

    // screencopy backend
    struct zwlr_screencopy_manager_v1 *         screencopy_manager;

    // extcopy backend
    struct ext_image_copy_capture_manager_v1 *                      copy_capture_manager;
    struct ext_output_image_capture_source_manager_v1 *             output_capture_source_manager;
    struct ext_foreign_toplevel_image_capture_source_manager_v1 *   toplevel_capture_source_manager;
} ctx_wl_protocols_t;

#endif
