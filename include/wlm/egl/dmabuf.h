#ifndef WLM_EGL_DMABUF_H_
#define WLM_EGL_DMABUF_H_

#include <stdbool.h>
#include <stdint.h>
#include <GLES2/gl2.h>
#include <wlm/egl.h>

typedef struct ctx ctx_t;
typedef struct wlm_egl_format wlm_egl_format_t;

bool wlm_egl_dmabuf_import(ctx_t * ctx, wlm_dmabuf_t * dmabuf, const wlm_egl_format_t * format, bool invert_y, bool region_aware);

#endif
