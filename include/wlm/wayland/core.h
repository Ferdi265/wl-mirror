#ifndef WL_MIRROR_WAYLAND_CORE_H_
#define WL_MIRROR_WAYLAND_CORE_H_

#include <stdbool.h>
#include <stddef.h>
#include <wayland-client-core.h>
#include <libdecor.h>
#include <wlm/event/loop.h>

typedef struct ctx ctx_t;

typedef struct {
    // wayland display
    struct wl_display * display;

    // libdecor context
    struct libdecor * libdecor_context;

    // event loop handler
    wlm_event_loop_handler_t event_handler;

    // program state
    bool closing;

    bool init_called;
    bool init_done;
} ctx_wl_core_t;

void wlm_wayland_core_on_before_poll(ctx_t *);

void wlm_wayland_core_zero(ctx_t *);
void wlm_wayland_core_init(ctx_t *);
void wlm_wayland_core_cleanup(ctx_t *);

void wlm_wayland_core_request_close(ctx_t *);
bool wlm_wayland_core_is_closing(ctx_t *);

bool wlm_wayland_core_is_init_called(ctx_t *);
bool wlm_wayland_core_is_init_done(ctx_t *);

#endif
