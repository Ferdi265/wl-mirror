#ifndef WL_MIRROR_WAYLAND_PROTOCOLS_H_
#define WL_MIRROR_WAYLAND_PROTOCOLS_H_

#include <wayland-client-protocol.h>
#include "viewporter.h"
#include "fractional-scale-v1.h"
#include "xdg-shell.h"

struct ctx;

typedef struct ctx_wl_protocols {
    struct wl_compositor * compositor;
    struct wl_shm * shm;
    struct wp_viewporter * viewporter;
    struct wp_fractional_scale_manager_v1 * fractional_scale_manager;
    struct xdg_wm_base * xdg_wm_base;
} ctx_wl_protocols_t;

void wayland_protocols_zero(struct ctx * ctx);
#define wayland_protocols_init(ctx)
#define wayland_protocols_cleanup(ctx)

#endif
