#include <string.h>
#include <wlm/context.h>
#include <wlm/util.h>
#include <wlm/util/transform.h>
#include "glsl_vertex_shader.h"
#include "glsl_fragment_shader.h"

#define WLM_LOG_COMPONENT egl

// --- buffers ---

static const float vertex_array[] = {
    -1.0, -1.0, 0.0, 0.0,
     1.0, -1.0, 1.0, 0.0,
    -1.0,  1.0, 0.0, 1.0,
    -1.0,  1.0, 0.0, 1.0,
     1.0, -1.0, 1.0, 0.0,
     1.0,  1.0, 1.0, 1.0
};

// --- private helper functions ---

static void redraw(ctx_t * ctx) {
    glClear(GL_COLOR_BUFFER_BIT);

    GLint texture = ctx->egl.render.texture /* TODO: ctx->opt.freeze ? ctx->egl.freeze_texture : ctx->egl.texture */;
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawArrays(GL_TRIANGLES, 0, WLM_ARRAY_LENGTH(vertex_array));

    if (eglSwapBuffers(ctx->egl.core.display, ctx->egl.core.surface) != EGL_TRUE) {
        wlm_log(ctx, WLM_FATAL, "failed to swap buffers");
        wlm_exit_fail(ctx);
    }
}

static void update_viewport(ctx_t * ctx) {
    uint32_t win_width = ctx->wl.window.buffer_width;
    uint32_t win_height = ctx->wl.window.buffer_height;
    uint32_t tex_width = ctx->egl.render.tex_width;
    uint32_t tex_height = ctx->egl.render.tex_height;
    uint32_t view_width = win_width;
    uint32_t view_height = win_height;

    // TODO: viewport target output transform
    // TODO: viewport target region clamp
    // TODO: viewport user transform

    // select biggest width or height that fits and preserves aspect ratio
    float win_aspect = (float)win_width / win_height;
    float tex_aspect = (float)tex_width / tex_height;
    if (win_aspect > tex_aspect) {
        view_width = view_height * tex_aspect;
    } else if (win_aspect < tex_aspect) {
        view_height = view_width / tex_aspect;
    }

    // TODO: exact scale

    // updating GL viewport
    wlm_log(ctx, WLM_DEBUG, "resizing view to %dx%d", view_width, view_height);
    glViewport((win_width - view_width) / 2, (win_height - view_height) / 2, view_width, view_height);

    // recalculate texture transform
    wlm_util_mat3_t texture_transform;
    wlm_util_mat3_identity(&texture_transform);

    // TODO: calculate texture transform

    // set texture transform matrix uniform
    // - GL matrices are stored in column-major order, so transpose the matrix
    wlm_util_mat3_transpose(&texture_transform);
    glUniformMatrix3fv(ctx->egl.render.texture_transform_uniform, 1, false, (float *)texture_transform.data);
}

static void update_render_options(ctx_t * ctx) {
    // set invert colors uniform
    bool invert_colors = false; /* TODO: ctx->opt.invert_colors*/;
    glUniform1i(ctx->egl.render.invert_colors_uniform, invert_colors);

    // set texture scaling
    GLint gl_scaling_mode = GL_LINEAR; /* TODO: ctx->opt.scaling == SCALE_LINEAR ? GL_LINEAR : GL_NEAREST */

    glBindTexture(GL_TEXTURE_2D, ctx->egl.render.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_scaling_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_scaling_mode);

    glBindTexture(GL_TEXTURE_2D, ctx->egl.render.freeze_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_scaling_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_scaling_mode);
}

// --- internal event handlers ---

void wlm_egl_render_on_render_request_redraw(ctx_t * ctx) {
    redraw(ctx);
}

void wlm_egl_render_on_window_changed(ctx_t * ctx) {
    if (!(ctx->wl.window.changed & WLM_WAYLAND_WINDOW_BUFFER_SIZE_CHANGED)) return;

    update_viewport(ctx);
    redraw(ctx);
}

// --- initialization and cleanup ---

void wlm_egl_render_zero(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "zeroing");

    ctx->egl.render.vbo = 0;
    ctx->egl.render.texture = 0;
    ctx->egl.render.freeze_texture = 0;
    ctx->egl.render.freeze_framebuffer = 0;
    ctx->egl.render.shader_program = 0;
    ctx->egl.render.texture_transform_uniform = 0;
    ctx->egl.render.invert_colors_uniform = 0;

    ctx->egl.render.tex_width = 0;
    ctx->egl.render.tex_height = 0;
    ctx->egl.render.tex_gl_format = 0;
}

void wlm_egl_render_init(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "initializing");

    // create vertex buffer object
    glGenBuffers(1, &ctx->egl.render.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->egl.render.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertex_array, vertex_array, GL_STATIC_DRAW);

    // create textures
    glGenTextures(1, &ctx->egl.render.texture);
    glGenTextures(1, &ctx->egl.render.freeze_texture);

    // create freeze framebuffer
    glGenFramebuffers(1, &ctx->egl.render.freeze_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx->egl.render.freeze_framebuffer);
    glBindTexture(GL_TEXTURE_2D, ctx->egl.render.texture);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx->egl.render.texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // error log for shader compilation error messages
    GLint success;
    const char * shader_source = NULL;
    char errorLog[1024] = { 0 };

    // compile vertex shader
    shader_source = glsl_vertex_shader;
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &shader_source, NULL);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        glGetShaderInfoLog(vertex_shader, sizeof errorLog, NULL, errorLog);
        errorLog[strcspn(errorLog, "\n")] = '\0';
        wlm_log(ctx, WLM_FATAL, "%s", errorLog);
        glDeleteShader(vertex_shader);
        wlm_exit_fail(ctx);
    }

    // compile fragment shader
    shader_source = glsl_fragment_shader;
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &shader_source, NULL);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        glGetShaderInfoLog(fragment_shader, sizeof errorLog, NULL, errorLog);
        errorLog[strcspn(errorLog, "\n")] = '\0';
        wlm_log(ctx, WLM_FATAL, "%s", errorLog);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        wlm_exit_fail(ctx);
    }

    // create shader program and get pointers to shader uniforms
    ctx->egl.render.shader_program = glCreateProgram();
    glAttachShader(ctx->egl.render.shader_program, vertex_shader);
    glAttachShader(ctx->egl.render.shader_program, fragment_shader);
    glLinkProgram(ctx->egl.render.shader_program);
    glGetProgramiv(ctx->egl.render.shader_program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        wlm_log(ctx, WLM_FATAL, "failed to link shader program");
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(ctx->egl.render.shader_program);
        wlm_exit_fail(ctx);
    }

    // bind shader uniforms and clean up shader compilation info
    ctx->egl.render.texture_transform_uniform = glGetUniformLocation(ctx->egl.render.shader_program, "uTexTransform");
    ctx->egl.render.invert_colors_uniform = glGetUniformLocation(ctx->egl.render.shader_program, "uInvertColors");
    glUseProgram(ctx->egl.render.shader_program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // set GL clear color to back and set GL vertex layout
    glClearColor(0.0, 0.0, 0.0, 1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), (void *)(0 * sizeof (float)));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), (void *)(2 * sizeof (float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // initialize uniforms
    update_viewport(ctx);
    update_render_options(ctx);

    // trigger redraw
    wlm_log(ctx, WLM_DEBUG, "triggering initial redraw");
    redraw(ctx);
}

void wlm_egl_render_cleanup(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "cleaning up");

    if (ctx->egl.render.shader_program != 0) glDeleteProgram(ctx->egl.render.shader_program);
    if (ctx->egl.render.freeze_framebuffer != 0) glDeleteFramebuffers(1, &ctx->egl.render.freeze_framebuffer);
    if (ctx->egl.render.freeze_texture != 0) glDeleteTextures(1, &ctx->egl.render.freeze_texture);
    if (ctx->egl.render.texture != 0) glDeleteTextures(1, &ctx->egl.render.texture);
    if (ctx->egl.render.vbo != 0) glDeleteBuffers(1, &ctx->egl.render.vbo);

    wlm_egl_render_zero(ctx);
}
