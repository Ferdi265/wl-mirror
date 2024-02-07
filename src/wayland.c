#include <wlm/context.h>

#define WLM_LOG_COMPONENT wayland

void wlm_wayland_zero(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "zeroing");

    wlm_wayland_core_zero(ctx);
    wlm_wayland_registry_zero(ctx);
    wlm_wayland_output_zero(ctx);
    wlm_wayland_window_zero(ctx);
}

void wlm_wayland_init(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "initializing");

    // initialize core components
    wlm_wayland_core_init(ctx);
    wlm_wayland_registry_init(ctx);

    // other components are initialized in event handlers
    // when dependencies are ready
}

void wlm_wayland_cleanup(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "cleaning up");

    // cleanup components
    wlm_wayland_window_cleanup(ctx);
    wlm_wayland_output_cleanup(ctx);

    // cleanup core components last so everything that needs it is cleaned up
    wlm_wayland_registry_cleanup(ctx);
    wlm_wayland_core_cleanup(ctx);
}
