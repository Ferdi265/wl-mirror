#ifndef WL_MIRROR_EGL_H_
#define WL_MIRROR_EGL_H_

#include <stdint.h>
#include <stdbool.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

struct ctx;

#define MAX_PLANES 4
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t drm_format;
    size_t planes;

    int * fds;
    uint32_t * offsets;
    uint32_t * strides;
    uint64_t modifier;
} dmabuf_t;

typedef struct {
    uint32_t drm_format;
    size_t num_modifiers;
    uint64_t * modifiers;
} dmabuf_format_t;

typedef struct {
    size_t num_formats;
    dmabuf_format_t * formats;
} dmabuf_formats_t;

typedef struct ctx_egl {
    EGLDisplay display;
    EGLContext context;
    EGLConfig config;
    EGLSurface surface;
    struct wl_egl_window * window;

    // extension functions
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
    PFNEGLQUERYDMABUFFORMATSEXTPROC eglQueryDmaBufFormatsEXT;
    PFNEGLQUERYDMABUFMODIFIERSEXTPROC eglQueryDmaBufModifiersEXT;

    // supported dmabuf formats
    dmabuf_formats_t dmabuf_formats;

    // texture size
    uint32_t width;
    uint32_t height;
    uint32_t format;

    // gl objects
    GLuint vbo;
    GLuint texture;
    GLuint freeze_texture;
    GLuint freeze_framebuffer;
    GLuint shader_program;
    GLint texture_transform_uniform;
    GLint invert_colors_uniform;

    // state flags
    bool texture_region_aware;
    bool texture_initialized;
    bool initialized;
} ctx_egl_t;

void wlm_egl_init(struct ctx * ctx);
bool wlm_egl_query_dmabuf_formats(struct ctx * ctx);

void wlm_egl_draw_texture(struct ctx * ctx);
void wlm_egl_resize_viewport(struct ctx * ctx);
void wlm_egl_resize_window(struct ctx * ctx);
void wlm_egl_update_uniforms(struct ctx * ctx);
void wlm_egl_freeze_framebuffer(struct ctx * ctx);
bool wlm_egl_dmabuf_to_texture(struct ctx * ctx, dmabuf_t * dmabuf);

void wlm_egl_cleanup(struct ctx * ctx);

#endif
