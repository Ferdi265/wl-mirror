#ifndef WL_MIRROR_WLM_WAYLAND_SEAT_H_
#define WL_MIRROR_WLM_WAYLAND_SEAT_H_

#include <stdbool.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

typedef struct ctx ctx_t;

typedef struct {
    struct wl_list link;
    struct wl_seat * seat;
} wlm_wayland_seat_entry_t;

typedef struct {
    struct wl_list seat_list;

    bool init_called;
    bool init_done;
} ctx_wl_seat_t;

void wlm_wayland_seat_on_add(ctx_t *, struct wl_seat * seat);
void wlm_wayland_seat_on_remove(ctx_t *, struct wl_seat * seat);

void wlm_wayland_seat_zero(ctx_t *);
void wlm_wayland_seat_init(ctx_t *);
void wlm_wayland_seat_cleanup(ctx_t *);

bool wlm_wayland_seat_is_init_called(ctx_t *);
bool wlm_wayland_seat_is_init_done(ctx_t *);

#endif
