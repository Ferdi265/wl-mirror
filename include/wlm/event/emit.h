#ifndef WL_MIRROR_EVENT_EMIT_H_
#define WL_MIRROR_EVENT_EMIT_H_

#include <wlm/wayland/output.h>

typedef struct ctx ctx_t;

void wlm_event_emit_before_poll(ctx_t *);
void wlm_event_emit_registry_initial_sync(ctx_t *);
void wlm_event_emit_output_initial_sync(ctx_t *);
void wlm_event_emit_output_changed(ctx_t *, wlm_wayland_output_entry_t * entry);

#endif
