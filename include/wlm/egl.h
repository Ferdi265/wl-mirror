#ifndef WL_MIRROR_EGL_H_
#define WL_MIRROR_EGL_H_

#include <wlm/egl/core.h>
#include <wlm/egl/render.h>

typedef struct ctx ctx_t;

typedef struct {
    ctx_egl_core_t core;
    ctx_egl_render_t render;
} ctx_egl_t;

void wlm_egl_zero(ctx_t *);
void wlm_egl_init(ctx_t *);
void wlm_egl_cleanup(ctx_t *);

#endif
