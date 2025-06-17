#include <wlm/event/emit.h>
#include <wlm/wayland/core.h>

void wlm_event_emit_before_poll(ctx_t * ctx) {
    wlm_wayland_core_on_before_poll(ctx);
}

void wlm_event_emit_output_changed(ctx_t * ctx, wlm_wayland_output_entry_t * entry) {
    (void)ctx;
    (void)entry;
}
