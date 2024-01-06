#include "context.h"

void wayland_events_emit_registry_initial_sync(ctx_t * ctx) {
    wayland_output_on_registry_initial_sync(ctx);
    wayland_window_on_registry_initial_sync(ctx);
}

void wayland_events_emit_output_initial_sync(ctx_t * ctx) {
    wayland_window_on_output_initial_sync(ctx);
}

void wayland_events_emit_output_changed(ctx_t * ctx, wayland_output_entry_t * entry) {
    (void)ctx;
    (void)entry;
}
