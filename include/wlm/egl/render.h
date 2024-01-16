#ifndef WL_MIRROR_EGL_RENDER_H_
#define WL_MIRROR_EGL_RENDER_H_

#include <GLES2/gl2.h>

typedef struct ctx ctx_t;

typedef struct {
    GLuint vbo;
    GLuint texture;
    GLuint freeze_texture;
    GLuint freeze_framebuffer;
    GLuint shader_program;
    GLint texture_transform_uniform;
    GLint invert_colors_uniform;

    uint32_t tex_width;
    uint32_t tex_height;
    uint32_t tex_gl_format;
} ctx_egl_render_t;

void wlm_egl_render_on_window_initial_configure(ctx_t *);
void wlm_egl_render_on_window_changed(ctx_t *);

void wlm_egl_render_zero(ctx_t *);
void wlm_egl_render_init(ctx_t *);
void wlm_egl_render_cleanup(ctx_t *);

#endif
