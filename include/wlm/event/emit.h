#ifndef WLM_EVENT_EMIT_H_
#define WLM_EVENT_EMIT_H_

typedef struct ctx ctx_t;
typedef struct wlm_wayland_output_entry wlm_wayland_output_entry_t;
typedef struct wlm_wayland_toplevel_entry wlm_wayland_toplevel_entry_t;

void wlm_event_emit_before_poll(ctx_t * ctx);
void wlm_event_emit_registry_init_done(ctx_t * ctx);

void wlm_event_emit_output_added(ctx_t * ctx, wlm_wayland_output_entry_t * entry);
void wlm_event_emit_output_changed(ctx_t * ctx, wlm_wayland_output_entry_t * entry);
void wlm_event_emit_output_removed(ctx_t * ctx, wlm_wayland_output_entry_t * entry);

void wlm_event_emit_toplevel_added(ctx_t * ctx, wlm_wayland_toplevel_entry_t * entry);
void wlm_event_emit_toplevel_changed(ctx_t * ctx, wlm_wayland_toplevel_entry_t * entry);
void wlm_event_emit_toplevel_removed(ctx_t * ctx, wlm_wayland_toplevel_entry_t * entry);

void wlm_event_emit_window_init_done(ctx_t * ctx);
void wlm_event_emit_window_changed(ctx_t * ctx);
void wlm_event_emit_window_redraw(ctx_t * ctx);
#endif
