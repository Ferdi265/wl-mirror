#include <wlm/context.h>

void wlm_event_emit_before_poll(ctx_t * ctx) {
    wlm_wayland_core_on_before_poll(ctx);
}

void wlm_event_emit_registry_initial_sync(ctx_t * ctx) {
    wlm_wayland_output_on_registry_initial_sync(ctx);
    wlm_wayland_window_on_registry_initial_sync(ctx);
}

void wlm_event_emit_output_initial_sync(ctx_t * ctx) {
    wlm_wayland_window_on_output_initial_sync(ctx);
}

void wlm_event_emit_output_changed(ctx_t * ctx, wlm_wayland_output_entry_t * entry) {
    (void)ctx;
    (void)entry;
}
