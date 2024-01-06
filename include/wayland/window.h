#ifndef WL_MIRROR_WAYLAND_WINDOW_H_
#define WL_MIRROR_WAYLAND_WINDOW_H_

typedef struct ctx ctx_t;

typedef struct {

} ctx_wl_window_t;

void wayland_window_zero(ctx_t *);
void wayland_window_init(ctx_t *);
void wayland_window_cleanup(ctx_t *);

#endif
