#include <wlm/context.h>

void wlm_event_emit_before_poll(ctx_t * ctx) {
    wlm_wayland_window_on_before_poll(ctx);
    wlm_wayland_core_on_before_poll(ctx);
}

void wlm_event_emit_registry_init_done(ctx_t * ctx) {
    wlm_wayland_output_init(ctx);
    wlm_wayland_window_init(ctx);
}

void wlm_event_emit_output_init_done(ctx_t * ctx) {
    wlm_wayland_window_on_output_init_done(ctx);
}

void wlm_event_emit_output_changed(ctx_t * ctx, wlm_wayland_output_entry_t * entry) {
    wlm_wayland_window_on_output_changed(ctx, entry);
}

void wlm_event_emit_output_removed(ctx_t * ctx, wlm_wayland_output_entry_t * entry) {
    wlm_wayland_window_on_output_removed(ctx, entry);
}

void wlm_event_emit_window_init_done(ctx_t * ctx) {
    wlm_egl_init(ctx);
}

void wlm_event_emit_window_changed(ctx_t * ctx) {
    wlm_egl_core_on_window_changed(ctx);
}

void wlm_event_emit_render_request_redraw(ctx_t * ctx) {
    wlm_egl_render_on_render_request_redraw(ctx);
}
