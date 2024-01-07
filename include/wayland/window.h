#ifndef WL_MIRROR_WAYLAND_WINDOW_H_
#define WL_MIRROR_WAYLAND_WINDOW_H_

#include "wayland/output.h"

typedef struct ctx ctx_t;

typedef struct {
    struct wl_surface * surface;
    struct wp_viewport * viewport;
    struct wp_fractional_scale_v1 * fractional_scale;
    struct xdg_surface * xdg_surface;
    struct xdg_toplevel * xdg_toplevel;

    wayland_output_entry_t * current_output;
} ctx_wl_window_t;

void wayland_window_zero(ctx_t *);
void wayland_window_init(ctx_t *);
void wayland_window_cleanup(ctx_t *);

void wayland_window_on_registry_initial_sync(ctx_t *);
void wayland_window_on_output_initial_sync(ctx_t *);
void wayland_window_on_output_removed(ctx_t *, wayland_output_entry_t * entry);

#endif
