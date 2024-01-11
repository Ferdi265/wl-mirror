#include <wlm/context.h>

void wlm_egl_zero(ctx_t * ctx) {
    wlm_egl_core_zero(ctx);
    wlm_egl_render_zero(ctx);
}

void wlm_egl_init(ctx_t * ctx) {
    wlm_egl_core_init(ctx);
    wlm_egl_render_init(ctx);
}

void wlm_egl_cleanup(ctx_t * ctx) {
    wlm_egl_render_cleanup(ctx);
    wlm_egl_core_cleanup(ctx);
}
