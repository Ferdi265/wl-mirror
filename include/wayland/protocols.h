#ifndef WL_MIRROR_WAYLAND_PROTOCOLS_H_
#define WL_MIRROR_WAYLAND_PROTOCOLS_H_

#include <wayland-client-protocol.h>
#include "viewporter.h"
#include "fractional-scale-v1.h"
#include "xdg-shell.h"

typedef struct {
    // required
    struct wl_compositor *                      compositor;
    struct wp_viewporter *                      viewporter;
    struct xdg_wm_base *                        xdg_wm_base;
    struct zxdg_output_manager_v1 *             xdg_output_manager;

    // optional
    struct wl_shm *                             shm;
    struct wp_fractional_scale_manager_v1 *     fractional_scale_manager;
} ctx_wl_protocols_t;

#endif
