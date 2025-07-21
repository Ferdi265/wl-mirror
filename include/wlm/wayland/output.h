#ifndef WLM_WAYLAND_OUTPUT_H_
#define WLM_WAYLAND_OUTPUT_H_

#include <stdint.h>

typedef struct ctx ctx_t;

typedef struct ctx_wl_output {

} ctx_wl_output_t;

void wlm_wayland_output_zero(ctx_t * ctx);
void wlm_wayland_output_init(ctx_t * ctx);
void wlm_wayland_output_cleanup(ctx_t * ctx);

void wlm_wayland_output_on_add(ctx_t *, struct wl_output * output);
void wlm_wayland_output_on_remove(ctx_t *, struct wl_output * output);
void wlm_wayland_output_on_round_trip_done(ctx_t * ctx);

#endif
