#ifndef WLM_EGL_SHM_H_
#define WLM_EGL_SHM_H_

#include <stdbool.h>
#include <stdint.h>
#include <GLES2/gl2.h>

typedef struct ctx ctx_t;
typedef struct wlm_egl_format wlm_egl_format_t;

bool wlm_egl_shm_import(ctx_t * ctx, void * addr, const wlm_egl_format_t * format, uint32_t width, uint32_t height, uint32_t stride, bool invert_y);

#endif
