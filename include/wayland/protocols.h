#ifndef WL_MIRROR_WAYLAND_PROTOCOLS_H_
#define WL_MIRROR_WAYLAND_PROTOCOLS_H_

#include <wayland-client-protocol.h>
#include "viewporter.h"
#include "xdg-shell.h"
#include "xdg-output-unstable-v1.h"
#include "fractional-scale-v1.h"
#include "wlr-export-dmabuf-unstable-v1.h"
#include "wlr-screencopy-unstable-v1.h"

typedef struct {
    // required
    struct wl_compositor *                      compositor;
    struct wp_viewporter *                      viewporter;
    struct xdg_wm_base *                        xdg_wm_base;
    struct zxdg_output_manager_v1 *             xdg_output_manager;

    // optional
    struct wl_shm *                             shm;
    struct wp_fractional_scale_manager_v1 *     fractional_scale_manager;
    struct zwlr_export_dmabuf_manager_v1 *      export_dmabuf_manager;
    struct zwlr_screencopy_manager_v1 *         screencopy_manager;
} ctx_wl_protocols_t;

#endif
