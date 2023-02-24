#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "context.h"
#include "transform.h"
#include "glsl_vertex_shader.h"
#include "glsl_fragment_shader.h"

// --- buffers ---

const float vertex_array[] = {
    -1.0, -1.0, 0.0, 0.0,
     1.0, -1.0, 1.0, 0.0,
    -1.0,  1.0, 0.0, 1.0,
    -1.0,  1.0, 0.0, 1.0,
     1.0, -1.0, 1.0, 0.0,
     1.0,  1.0, 1.0, 1.0
};

// --- has_extension ---

static bool has_extension(const char * extension) {
    size_t ext_len = strlen(extension);

    // try to find extension in extension list
    const char * extensions = (const char *)glGetString(GL_EXTENSIONS);
    char * match = strstr(extensions, extension);

    // verify match was not a substring of another extension
    bool found = (
        match != NULL &&
        (match == extensions || match[-1] == ' ') &&
        (match[ext_len] == '\0' || match[ext_len] == ' ')
    );

    return found;
}

// --- init_egl ---

void init_egl(ctx_t * ctx) {
    // initialize context structure
    ctx->egl.display = EGL_NO_DISPLAY;
    ctx->egl.context = EGL_NO_CONTEXT;
    ctx->egl.surface = EGL_NO_SURFACE;
    ctx->egl.window = EGL_NO_SURFACE;

    ctx->egl.glEGLImageTargetTexture2DOES = NULL;

    ctx->egl.width = 1;
    ctx->egl.height = 1;
    ctx->egl.format = 0;

    ctx->egl.vbo = 0;
    ctx->egl.texture = 0;
    ctx->egl.freeze_texture = 0;
    ctx->egl.freeze_framebuffer = 0;
    ctx->egl.shader_program = 0;
    ctx->egl.texture_transform_uniform = 0;
    ctx->egl.invert_colors_uniform = 0;

    ctx->egl.texture_region_aware = false;
    ctx->egl.texture_initialized = false;
    ctx->egl.initialized = true;

    // create egl display
    ctx->egl.display = eglGetDisplay((EGLNativeDisplayType)ctx->wl.display);
    if (ctx->egl.display == EGL_NO_DISPLAY) {
        log_error("egl::init(): failed to create EGL display\n");
        exit_fail(ctx);
    }

    // initialize egl display and check egl version
    EGLint major, minor;
    if (eglInitialize(ctx->egl.display, &major, &minor) != EGL_TRUE) {
        log_error("egl::init(): failed to initialize EGL display\n");
        exit_fail(ctx);
    }
    log_debug(ctx, "egl::init(): initialized EGL %d.%d\n", major, minor);

    // find an egl config with
    // - window support
    // - OpenGL ES 2.0 support
    // - RGB888 texture support
    EGLint num_configs;
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    if (eglChooseConfig(ctx->egl.display, config_attribs, &ctx->egl.config, 1, &num_configs) != EGL_TRUE) {
        log_error("egl::init(): failed to get EGL config\n");
        exit_fail(ctx);
    }

    // default window size to 100x100 if not set
    if (ctx->wl.width == 0) ctx->wl.width = 100;
    if (ctx->wl.height == 0) ctx->wl.height = 100;

    // create egl window
    ctx->egl.window = wl_egl_window_create(ctx->wl.surface, ctx->wl.width, ctx->wl.height);
    if (ctx->egl.window == EGL_NO_SURFACE) {
        log_error("egl::init(): failed to create EGL window\n");
        exit_fail(ctx);
    }

    // create egl surface
    ctx->egl.surface = eglCreateWindowSurface(ctx->egl.display, ctx->egl.config, ctx->egl.window, NULL);

    // create egl context with support for OpenGL ES 2.0
    EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 2,
        EGL_CONTEXT_MINOR_VERSION, 0,
        EGL_NONE
    };
    ctx->egl.context = eglCreateContext(ctx->egl.display, ctx->egl.config, EGL_NO_CONTEXT, context_attribs);
    if (ctx->egl.context == EGL_NO_CONTEXT) {
        log_error("egl::init(): failed to create EGL context\n");
        exit_fail(ctx);
    }

    // activate egl context
    if (eglMakeCurrent(ctx->egl.display, ctx->egl.surface, ctx->egl.surface, ctx->egl.context) != EGL_TRUE) {
        log_error("egl::init(): failed to activate EGL context\n");
        exit_fail(ctx);
    }

    // check for needed extensions
    // - GL_OES_EGL_image: for converting EGLImages to GL textures
    if (!has_extension("GL_OES_EGL_image")) {
        log_error("egl::init(): missing EGL extension GL_OES_EGL_image\n");
        exit_fail(ctx);
    }

    // get pointers to functions provided by extensions
    // - glEGLImageTargetTexture2DOES: for converting EGLImages to GL textures
    ctx->egl.glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if (ctx->egl.glEGLImageTargetTexture2DOES == NULL) {
        log_error("egl::init(): failed to get pointer to glEGLImageTargetTexture2DOES\n");
        exit_fail(ctx);
    }

    // create vertex buffer object
    glGenBuffers(1, &ctx->egl.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->egl.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertex_array, vertex_array, GL_STATIC_DRAW);

    // create texture and set scaling mode
    glGenTextures(1, &ctx->egl.texture);
    glBindTexture(GL_TEXTURE_2D, ctx->egl.texture);
    if (ctx->opt.scaling == SCALE_LINEAR) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // create freeze texture and set scaling mode
    glGenTextures(1, &ctx->egl.freeze_texture);
    glBindTexture(GL_TEXTURE_2D, ctx->egl.freeze_texture);
    if (ctx->opt.scaling == SCALE_LINEAR) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // create freeze framebuffer
    glGenFramebuffers(1, &ctx->egl.freeze_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx->egl.freeze_framebuffer);
    glBindTexture(GL_TEXTURE_2D, ctx->egl.texture);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx->egl.texture, 0);
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
        log_error("egl::init(): failed to compile vertex shader: %s\n", errorLog);
        glDeleteShader(vertex_shader);
        exit_fail(ctx);
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
        log_error("egl::init(): failed to compile fragment shader: %s\n", errorLog);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        exit_fail(ctx);
    }

    // create shader program and get pointers to shader uniforms
    ctx->egl.shader_program = glCreateProgram();
    glAttachShader(ctx->egl.shader_program, vertex_shader);
    glAttachShader(ctx->egl.shader_program, fragment_shader);
    glLinkProgram(ctx->egl.shader_program);
    glGetProgramiv(ctx->egl.shader_program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        log_error("egl::init(): failed to link shader program\n");
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(ctx->egl.shader_program);
        exit_fail(ctx);
    }
    ctx->egl.texture_transform_uniform = glGetUniformLocation(ctx->egl.shader_program, "uTexTransform");
    ctx->egl.invert_colors_uniform = glGetUniformLocation(ctx->egl.shader_program, "uInvertColors");
    glUseProgram(ctx->egl.shader_program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // set initial texture transform matrix
    mat3_t texture_transform;
    mat3_identity(&texture_transform);
    glUniformMatrix3fv(ctx->egl.texture_transform_uniform, 1, false, (float *)texture_transform.data);

    // set invert colors uniform
    bool invert_colors = ctx->opt.invert_colors;
    glUniform1i(ctx->egl.invert_colors_uniform, invert_colors);

    // set GL clear color to back and set GL vertex layout
    glClearColor(0.0, 0.0, 0.0, 1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), (void *)(0 * sizeof (float)));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), (void *)(2 * sizeof (float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // draw initial frame
    draw_texture(ctx);
    if (eglSwapBuffers(ctx->egl.display, ctx->egl.surface) != EGL_TRUE) {
        log_error("egl::init(): failed to swap buffers\n");
        exit_fail(ctx);
    }
}

// --- draw_texture ---

void draw_texture(ctx_t *ctx) {
    glBindTexture(GL_TEXTURE_2D, ctx->opt.freeze ? ctx->egl.freeze_texture : ctx->egl.texture);
    glClear(GL_COLOR_BUFFER_BIT);

    if (ctx->egl.texture_initialized) {
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}

// --- resize_viewport

void resize_viewport(ctx_t * ctx) {
    log_debug(ctx, "egl::resize_viewport(): resizing viewport\n");

    uint32_t win_width = ctx->wl.scale * ctx->wl.width;
    uint32_t win_height = ctx->wl.scale * ctx->wl.height;
    uint32_t tex_width = ctx->egl.width;
    uint32_t tex_height = ctx->egl.height;
    uint32_t view_width = win_width;
    uint32_t view_height = win_height;

    // rotate texture dimensions by output transform
    if (ctx->egl.texture_initialized) {
        viewport_apply_output_transform(&tex_width, &tex_height, ctx->mirror.current_target->transform);
    }

    // clamp texture dimensions to specified region
    region_t output_region;
    region_t clamp_region;
    if (ctx->egl.texture_initialized && ctx->opt.has_region && !ctx->egl.texture_region_aware) {
        output_region = (region_t){
            .x = 0, .y = 0,
            .width = tex_width, .height = tex_height
        };
        clamp_region = ctx->mirror.current_region;

        region_scale(&clamp_region, ctx->mirror.current_target->scale);
        region_clamp(&clamp_region, &output_region);

        tex_width = clamp_region.width;
        tex_height = clamp_region.height;
    }

    // rotate texture dimensions by user transform
    viewport_apply_transform(&tex_width, &tex_height, ctx->opt.transform);

    // select biggest width or height that fits and preserves aspect ratio
    float win_aspect = (float)win_width / win_height;
    float tex_aspect = (float)tex_width / tex_height;
    if (win_aspect > tex_aspect) {
        view_width = view_height * tex_aspect;
    } else if (win_aspect < tex_aspect) {
        view_height = view_width / tex_aspect;
    }

    // find biggest fitting integer scale
    if (ctx->opt.scaling == SCALE_EXACT) {
        uint32_t width_scale = win_width / tex_width;
        uint32_t height_scale = win_height / tex_height;
        uint32_t scale = width_scale < height_scale ? width_scale : height_scale;
        if (scale > 0) {
            view_width = scale * tex_width;
            view_height = scale * tex_height;
        }
    }

    // updating GL viewport
    glViewport((win_width - view_width) / 2, (win_height - view_height) / 2, view_width, view_height);

    // recalculate texture transform
    mat3_t texture_transform;
    mat3_identity(&texture_transform);
    if (ctx->egl.texture_initialized) {
        // apply transformations in reverse order as we need to transform
        // from OpenGL space to texture space

        mat3_apply_invert_y(&texture_transform, true);
        mat3_apply_transform(&texture_transform, ctx->opt.transform);

        if (ctx->opt.has_region && !ctx->egl.texture_region_aware) {
            mat3_apply_region_transform(&texture_transform, &clamp_region, &output_region);
        }

        mat3_apply_output_transform(&texture_transform, ctx->mirror.current_target->transform);
        mat3_apply_invert_y(&texture_transform, ctx->mirror.invert_y);
    }

    // set texture transform matrix uniform
    // - GL matrices are stored in column-major order, so transpose the matrix
    mat3_transpose(&texture_transform);
    glUniformMatrix3fv(ctx->egl.texture_transform_uniform, 1, false, (float *)texture_transform.data);
}

// --- resize_window ---

void resize_window(ctx_t * ctx) {
    log_debug(ctx, "egl::resize_window(): resizing EGL window\n");

    // resize window, then trigger viewport recalculation
    wl_egl_window_resize(ctx->egl.window, ctx->wl.scale * ctx->wl.width, ctx->wl.scale * ctx->wl.height, 0, 0);
    resize_viewport(ctx);

    // redraw frame
    draw_texture(ctx);
    if (eglSwapBuffers(ctx->egl.display, ctx->egl.surface) != EGL_TRUE) {
        log_error("egl::resize_window(): failed to swap buffers\n");
        exit_fail(ctx);
    }
}

// --- update_uniforms ---

void update_uniforms(ctx_t * ctx) {
    // trigger viewport recalculation
    resize_viewport(ctx);

    // set invert colors uniform
    bool invert_colors = ctx->opt.invert_colors;
    glUniform1i(ctx->egl.invert_colors_uniform, invert_colors);

    // set texture scaling mode
    if (ctx->opt.scaling == SCALE_LINEAR) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

}

// --- freeze_framebuffer ---

void freeze_framebuffer(struct ctx * ctx) {
    glBindFramebuffer(GL_FRAMEBUFFER, ctx->egl.freeze_framebuffer);
    glBindTexture(GL_TEXTURE_2D, ctx->egl.freeze_texture);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, ctx->egl.format, 0, 0, ctx->egl.width, ctx->egl.height, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// --- cleanup_egl ---

void cleanup_egl(ctx_t *ctx) {
    if (!ctx->egl.initialized) return;

    log_debug(ctx, "egl::cleanup(): destroying EGL objects\n");

    if (ctx->egl.shader_program != 0) glDeleteProgram(ctx->egl.shader_program);
    if (ctx->egl.freeze_framebuffer != 0) glDeleteFramebuffers(1, &ctx->egl.freeze_framebuffer);
    if (ctx->egl.freeze_texture != 0) glDeleteTextures(1, &ctx->egl.freeze_texture);
    if (ctx->egl.texture != 0) glDeleteTextures(1, &ctx->egl.texture);
    if (ctx->egl.vbo != 0) glDeleteBuffers(1, &ctx->egl.vbo);
    if (ctx->egl.context != EGL_NO_CONTEXT) eglDestroyContext(ctx->egl.display, ctx->egl.context);
    if (ctx->egl.surface != EGL_NO_SURFACE) eglDestroySurface(ctx->egl.display, ctx->egl.surface);
    if (ctx->egl.window != EGL_NO_SURFACE) wl_egl_window_destroy(ctx->egl.window);
    if (ctx->egl.display != EGL_NO_DISPLAY) eglTerminate(ctx->egl.display);

    ctx->egl.initialized = false;
}
