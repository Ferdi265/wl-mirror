#ifndef WL_MIRROR_WAYLAND_CORE_H_
#define WL_MIRROR_WAYLAND_CORE_H_

#include <stdbool.h>
#include <stddef.h>
#include <wayland-client-core.h>
#include "event.h"

struct ctx;

typedef struct ctx_wl_core {
    // wayland display
    struct wl_display * display;
    bool closing;

    // event loop handler
    event_handler_t event_handler;
} ctx_wl_core_t;

void wayland_core_zero(struct ctx * ctx);
void wayland_core_init(struct ctx * ctx);
void wayland_core_cleanup(struct ctx * ctx);
bool wayland_core_is_closing(struct ctx * ctx);

#endif
