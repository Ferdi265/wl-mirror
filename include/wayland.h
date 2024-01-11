#ifndef WL_MIRROR_WAYLAND_H_
#define WL_MIRROR_WAYLAND_H_

#include "wayland/core.h"
#include "wayland/registry.h"
#include "wayland/protocols.h"
#include "wayland/output.h"

struct ctx;

typedef struct ctx_wl {
    ctx_wl_core_t core;
    ctx_wl_registry_t registry;
    ctx_wl_protocols_t protocols;
    ctx_wl_output_t output;
} ctx_wl_t;

void wayland_zero(struct ctx * ctx);
void wayland_init(struct ctx * ctx);
void wayland_cleanup(struct ctx * ctx);

#endif
