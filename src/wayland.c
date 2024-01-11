#include <wlm/context.h>

void wlm_wayland_zero(ctx_t * ctx) {
    wlm_wayland_core_zero(ctx);
    wlm_wayland_registry_zero(ctx);
    wlm_wayland_output_zero(ctx);
    wlm_wayland_window_zero(ctx);
}

void wlm_wayland_init(ctx_t * ctx) {
    // initialize core components
    wlm_wayland_core_init(ctx);

    // initialize components
    wlm_wayland_output_init(ctx);
    wlm_wayland_window_init(ctx);

    // initialize registry last so everything is ready for event handlers
    wlm_wayland_registry_init(ctx);
}

void wlm_wayland_cleanup(ctx_t * ctx) {
    // cleanup components
    wlm_wayland_window_cleanup(ctx);
    wlm_wayland_output_cleanup(ctx);

    // cleanup core components last so everything that needs it is cleaned up
    wlm_wayland_registry_cleanup(ctx);
    wlm_wayland_core_cleanup(ctx);
}
