#ifndef WL_MIRROR_EGL_CORE_H_
#define WL_MIRROR_EGL_CORE_H_

#include <stdbool.h>
#include <wayland-egl.h>
#include <EGL/egl.h>

typedef struct ctx ctx_t;

typedef struct {
    // egl context
    EGLDisplay display;
    EGLConfig config;
    EGLContext context;

    // egl window system integration
    struct wl_egl_window * window;
    EGLSurface surface;

    bool init_called;
    bool init_done;
} ctx_egl_core_t;

void wlm_egl_core_zero(ctx_t *);
void wlm_egl_core_init(ctx_t *);
void wlm_egl_core_cleanup(ctx_t *);

void wlm_egl_core_on_window_changed(ctx_t *);

bool wlm_egl_core_is_init_called(ctx_t *);
bool wlm_egl_core_is_init_done(ctx_t *);

#endif
