#pragma once

#include "wayland/output.h"

typedef struct ctx ctx_t;

void wayland_events_emit_registry_initial_sync(ctx_t * ctx);
void wayland_events_emit_output_initial_sync(ctx_t * ctx);
void wayland_events_emit_output_changed(ctx_t * ctx, wayland_output_entry_t * entry);
