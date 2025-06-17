#ifndef WLM_WAYLAND_CORE_H_
#define WLM_WAYLAND_CORE_H_

#include <wlm/event.h>

typedef struct ctx ctx_t;
typedef struct ctx_wl_core ctx_wl_core_t;

struct ctx_wl_core {
    struct wl_display * display;

#ifdef WITH_LIBDECOR
    struct libdecor * libdecor_context;
#endif

    wlm_event_loop_handler_t event_handler;

    // state
    bool closing;
    bool init_called;
    bool init_done;
};

// --- internal event handlers ---

void wlm_wayland_core_on_before_poll(ctx_t *);

// --- initialization and cleanup ---

void wlm_wayland_core_zero(ctx_t *);
void wlm_wayland_core_init(ctx_t *);
void wlm_wayland_core_cleanup(ctx_t *);

// --- public functions ---

void wlm_wayland_core_request_close(ctx_t *);

// --- getters ---

struct wl_display * wlm_wayland_core_get_display(ctx_t *);

#ifdef WITH_LIBDECOR
struct libdecor * wlm_wayland_core_get_libdecor_context(ctx_t *);
#endif

bool wlm_wayland_core_is_closing(ctx_t *);
bool wlm_wayland_core_is_init_called(ctx_t *);
bool wlm_wayland_core_is_init_done(ctx_t *);

#endif
