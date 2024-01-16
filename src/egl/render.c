#include <string.h>
#include <wlm/context.h>
#include "glsl_vertex_shader.h"
#include "glsl_fragment_shader.h"

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
    // TODO: actually draw
    glClear(GL_COLOR_BUFFER_BIT);
    if (eglSwapBuffers(ctx->egl.core.display, ctx->egl.core.surface) != EGL_TRUE) {
        log_error("egl::render::redraw(): failed to swap buffers\n");
        wlm_exit_fail(ctx);
    }
}

// --- internal event handlers ---

void wlm_egl_render_on_render_request_redraw(ctx_t * ctx) {
    redraw(ctx);
}

// --- initialization and cleanup ---

void wlm_egl_render_zero(ctx_t * ctx) {
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
    // create vertex buffer object
    glGenBuffers(1, &ctx->egl.render.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->egl.render.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertex_array, vertex_array, GL_STATIC_DRAW);

    // create texture and set scaling mode
    glGenTextures(1, &ctx->egl.render.texture);
    glBindTexture(GL_TEXTURE_2D, ctx->egl.render.texture);

    if (true /* TODO: ctx->opt.scaling == SCALE_LINEAR */) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // create freeze texture and set scaling mode
    glGenTextures(1, &ctx->egl.render.freeze_texture);
    glBindTexture(GL_TEXTURE_2D, ctx->egl.render.freeze_texture);
    if (true /* TODO: ctx->opt.scaling == SCALE_LINEAR */) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

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
        log_error("egl::render::init(): failed to compile vertex shader: %s\n", errorLog);
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
        log_error("egl::render::init(): failed to compile fragment shader: %s\n", errorLog);
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
        log_error("egl::render::init(): failed to link shader program\n");
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

    // trigger redraw
    log_debug(ctx, "egl::render::init(): triggering initial redraw\n");
    redraw(ctx);
}

void wlm_egl_render_cleanup(ctx_t * ctx) {
    if (ctx->egl.render.shader_program != 0) glDeleteProgram(ctx->egl.render.shader_program);
    if (ctx->egl.render.freeze_framebuffer != 0) glDeleteFramebuffers(1, &ctx->egl.render.freeze_framebuffer);
    if (ctx->egl.render.freeze_texture != 0) glDeleteTextures(1, &ctx->egl.render.freeze_texture);
    if (ctx->egl.render.texture != 0) glDeleteTextures(1, &ctx->egl.render.texture);
    if (ctx->egl.render.vbo != 0) glDeleteBuffers(1, &ctx->egl.render.vbo);

    wlm_egl_render_zero(ctx);
}
