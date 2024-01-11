#ifndef WL_MIRROR_EGL_RENDER_H_
#define WL_MIRROR_EGL_RENDER_H_

typedef struct ctx ctx_t;

typedef struct {

} ctx_egl_render_t;

void wlm_egl_render_zero(ctx_t *);
void wlm_egl_render_init(ctx_t *);
void wlm_egl_render_cleanup(ctx_t *);

#endif
