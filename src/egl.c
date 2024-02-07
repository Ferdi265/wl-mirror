#include <wlm/context.h>

#define WLM_LOG_COMPONENT egl

void wlm_egl_zero(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "zeroing");

    wlm_egl_core_zero(ctx);
    wlm_egl_render_zero(ctx);
}

void wlm_egl_init(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "initializing");

    wlm_egl_core_init(ctx);
    wlm_egl_render_init(ctx);
}

void wlm_egl_cleanup(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "cleaning up");

    wlm_egl_render_cleanup(ctx);
    wlm_egl_core_cleanup(ctx);
}
