#ifndef WLM_WAYLAND_SEAT_H_
#define WLM_WAYLAND_SEAT_H_

#include <stdint.h>

typedef struct ctx ctx_t;

typedef struct ctx_wl_seat {

} ctx_wl_seat_t;

void wlm_wayland_seat_zero(ctx_t * ctx);
void wlm_wayland_seat_init(ctx_t * ctx);
void wlm_wayland_seat_cleanup(ctx_t * ctx);

void wlm_wayland_seat_on_add(ctx_t *, struct wl_seat * seat);
void wlm_wayland_seat_on_remove(ctx_t *, struct wl_seat * seat);

#endif
