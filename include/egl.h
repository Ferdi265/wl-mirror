#ifndef WL_MIRROR_EGL_H_
#define WL_MIRROR_EGL_H_

#include <stdint.h>
#include <stdbool.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

struct ctx;

typedef struct ctx_egl {
    EGLDisplay display;
    EGLContext context;
    EGLConfig config;
    EGLSurface surface;
    struct wl_egl_window * window;

    // extension functions
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

    // texture size
    uint32_t width;
    uint32_t height;

    // gl objects
    GLuint vbo;
    GLuint texture;
    GLuint shader_program;
    GLint texture_transform_uniform;
    bool texture_initialized;
} ctx_egl_t;

void init_egl(struct ctx * ctx);

void draw_texture_egl(struct ctx * ctx);
void resize_viewport_egl(struct ctx * ctx);
void resize_window_egl(struct ctx * ctx);

void cleanup_egl(struct ctx * ctx);

#endif
