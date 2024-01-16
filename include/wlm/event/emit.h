#ifndef WL_MIRROR_EVENT_EMIT_H_
#define WL_MIRROR_EVENT_EMIT_H_

#include <wlm/wayland/output.h>

typedef struct ctx ctx_t;

void wlm_event_emit_before_poll(ctx_t *);
void wlm_event_emit_registry_init_done(ctx_t *);
void wlm_event_emit_output_init_done(ctx_t *);
void wlm_event_emit_output_changed(ctx_t *, wlm_wayland_output_entry_t * entry);
void wlm_event_emit_output_removed(ctx_t *, wlm_wayland_output_entry_t * entry);
void wlm_event_emit_window_init_done(ctx_t *);
void wlm_event_emit_window_changed(ctx_t *);
void wlm_event_emit_render_request_redraw(ctx_t *);

#endif
