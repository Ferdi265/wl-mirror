#ifndef WL_MIRROR_WAYLAND_OUTPUT_H_
#define WL_MIRROR_WAYLAND_OUTPUT_H_

#include <wayland-client-protocol.h>

struct ctx;

typedef struct ctx_wl_output {

} ctx_wl_output_t;

void wayland_output_zero(struct ctx * ctx);
void wayland_output_init(struct ctx * ctx);
void wayland_output_cleanup(struct ctx * ctx);
void wayland_output_on_add(struct ctx * ctx, struct wl_output * output);
void wayland_output_on_remove(struct ctx * ctx, struct wl_output * output);

#endif
