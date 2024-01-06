#include "context.h"

void wayland_zero(ctx_t * ctx) {
    wayland_core_zero(ctx);
    wayland_registry_zero(ctx);
    wayland_output_zero(ctx);
    wayland_window_zero(ctx);
}

void wayland_init(ctx_t * ctx) {
    // initialize core components
    wayland_core_init(ctx);

    // initialize components
    wayland_output_init(ctx);
    wayland_window_init(ctx);

    // initialize registry last so everything is ready for event handlers
    wayland_registry_init(ctx);
}

void wayland_cleanup(ctx_t * ctx) {
    // cleanup components
    wayland_window_cleanup(ctx);
    wayland_output_cleanup(ctx);

    // cleanup core components last so everything that needs it is cleaned up
    wayland_registry_cleanup(ctx);
    wayland_core_cleanup(ctx);

    wayland_zero(ctx);
}
