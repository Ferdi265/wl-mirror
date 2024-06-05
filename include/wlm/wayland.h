#ifndef WL_MIRROR_WAYLAND_H_
#define WL_MIRROR_WAYLAND_H_

#include <wlm/wayland/core.h>
#include <wlm/wayland/registry.h>
#include <wlm/wayland/protocols.h>
#include <wlm/wayland/output.h>
#include <wlm/wayland/seat.h>
#include <wlm/wayland/window.h>

typedef struct ctx ctx_t;

typedef struct {
    ctx_wl_core_t core;
    ctx_wl_registry_t registry;
    ctx_wl_protocols_t protocols;
    ctx_wl_output_t output;
    ctx_wl_seat_t seat;
    ctx_wl_window_t window;
} ctx_wl_t;

void wlm_wayland_zero(ctx_t *);
void wlm_wayland_init(ctx_t *);
void wlm_wayland_cleanup(ctx_t *);

#endif
