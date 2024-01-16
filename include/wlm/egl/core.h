#ifndef WL_MIRROR_EGL_CORE_H_
#define WL_MIRROR_EGL_CORE_H_

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
} ctx_egl_core_t;

void wlm_egl_core_zero(ctx_t *);
void wlm_egl_core_init(ctx_t *);
void wlm_egl_core_cleanup(ctx_t *);

void wlm_egl_core_on_window_changed(ctx_t *);

#endif
