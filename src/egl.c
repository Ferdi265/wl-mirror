#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "context.h"

// --- shaders ---

const char * vertex_shader_source =
    "#version 100\n"
    "uniform bool uInvertY;\n"
    "attribute vec2 aPosition;\n"
    "attribute vec2 aTexCoord;\n"
    "varying vec2 vTexCoord;\n"
    "void main() {\n"
    "    float y = aPosition.y;\n"
    "    if (uInvertY) y = -y;\n"
    "    gl_Position = vec4(aPosition.x, y, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "}\n"
;

const char * fragment_shader_source =
    "#version 100\n"
    "precision mediump float;\n"
    "uniform sampler2D uTexture;\n"
    "varying vec2 vTexCoord;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(uTexture, vTexCoord);\n"
    "}\n"
;

// --- buffers ---

const float vertex_array[] = {
    -1.0, -1.0, 0.0, 1.0,
     1.0, -1.0, 1.0, 1.0,
    -1.0,  1.0, 0.0, 0.0,
    -1.0,  1.0, 0.0, 0.0,
     1.0, -1.0, 1.0, 1.0,
     1.0,  1.0, 1.0, 0.0
};

// --- has_extension ---

static bool has_extension(const char * extension) {
    size_t ext_len = strlen(extension);

    const char * extensions = (const char *)glGetString(GL_EXTENSIONS);
    char * match = strstr(extensions, extension);

    bool found = (
        match != NULL &&
        (match == extensions || match[-1] == ' ') &&
        (match[ext_len] == '\0' || match[ext_len] == ' ')
    );

    return found;
}

// --- init_egl ---

void init_egl(ctx_t * ctx) {
    printf("[info] init_egl: allocating context structure\n");
    ctx->egl = malloc(sizeof (ctx_egl_t));
    if (ctx->egl == NULL) {
        printf("[error] init_egl: failed to allocate context structure\n");
        exit_fail(ctx);
    }

    ctx->egl->display = EGL_NO_DISPLAY;
    ctx->egl->context = EGL_NO_CONTEXT;
    ctx->egl->surface = EGL_NO_SURFACE;
    ctx->egl->window = EGL_NO_SURFACE;

    ctx->egl->glEGLImageTargetTexture2DOES = NULL;

    ctx->egl->width = 1;
    ctx->egl->height = 1;

    ctx->egl->vbo = 0;
    ctx->egl->texture = 0;
    ctx->egl->shader_program = 0;
    ctx->egl->invert_y_uniform = 0;
    ctx->egl->texture_initialized = false;
    ctx->egl->invert_y = false;

    printf("[info] init_egl: creating EGL display\n");
    ctx->egl->display = eglGetDisplay((EGLNativeDisplayType)ctx->wl->display);
    if (ctx->egl->display == EGL_NO_DISPLAY) {
        printf("[error] init_egl: failed to create EGL display\n");
        exit_fail(ctx);
    }

    EGLint major, minor;
    printf("[info] init_egl: initializing EGL display\n");
    if (eglInitialize(ctx->egl->display, &major, &minor) != EGL_TRUE) {
        printf("[error] init_egl: failed to initialize EGL display\n");
        exit_fail(ctx);
    }
    printf("[info] init_egl: initialized EGL %d.%d\n", major, minor);

    EGLint num_configs;
    printf("[info] init_egl: getting number of EGL configs\n");
    if (eglGetConfigs(ctx->egl->display, NULL, 0, &num_configs) != EGL_TRUE) {
        printf("[error] init_egl: failed to get number of EGL configs\n");
        exit_fail(ctx);
    }

    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    printf("[info] init_egl: getting EGL config\n");
    if (eglChooseConfig(ctx->egl->display, config_attribs, &ctx->egl->config, 1, &num_configs) != EGL_TRUE) {
        printf("[error] init_egl: failed to get EGL config\n");
        exit_fail(ctx);
    }

    if (ctx->wl->width == 0) ctx->wl->width = 100;
    if (ctx->wl->height == 0) ctx->wl->height = 100;
    printf("[info] init_egl: creating EGL window\n");
    ctx->egl->window = wl_egl_window_create(ctx->wl->surface, ctx->wl->width, ctx->wl->height);
    if (ctx->egl->window == EGL_NO_SURFACE) {
        printf("[error] init_egl: failed to create EGL window\n");
        exit_fail(ctx);
    }

    printf("[info] init_egl: creating EGL surface\n");
    ctx->egl->surface = eglCreateWindowSurface(ctx->egl->display, ctx->egl->config, ctx->egl->window, NULL);

    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    printf("[info] init_egl: creating EGL context\n");
    ctx->egl->context = eglCreateContext(ctx->egl->display, ctx->egl->config, EGL_NO_CONTEXT, context_attribs);
    if (ctx->egl->context == EGL_NO_CONTEXT) {
        printf("[error] init_egl: failed to create EGL context\n");
        exit_fail(ctx);
    }

    printf("[info] init_egl: activating EGL context\n");
    if (eglMakeCurrent(ctx->egl->display, ctx->egl->surface, ctx->egl->surface, ctx->egl->context) != EGL_TRUE) {
        printf("[error] init_egl: failed to activate EGL context\n");
        exit_fail(ctx);
    }

    printf("[info] init_egl: checking for needed EGL extensions\n");
    if (!has_extension("GL_OES_EGL_image")) {
        printf("[error] init_egl: missing EGL extension GL_OES_EGL_image\n");
        exit_fail(ctx);
    }

    printf("[info] init_egl: getting extension function pointers\n");
    ctx->egl->glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if (ctx->egl->glEGLImageTargetTexture2DOES == NULL) {
        printf("[error] init_egl: failed to get pointer to glEGLImageTargetTexture2DOES\n");
        exit_fail(ctx);
    }

    printf("[info] init_egl: creating vertex buffer object\n");
    glGenBuffers(1, &ctx->egl->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->egl->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertex_array, vertex_array, GL_STATIC_DRAW);

    printf("[info] init_egl: creating texture\n");
    glGenTextures(1, &ctx->egl->texture);
    glBindTexture(GL_TEXTURE_2D, ctx->egl->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLint success;
    char errorLog[1024] = { 0 };

    printf("[info] init_egl: creating vertex shader\n");
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        glGetShaderInfoLog(vertex_shader, sizeof errorLog, NULL, errorLog);
        errorLog[strcspn(errorLog, "\n")] = '\0';
        printf("[error] init_egl: failed to compile vertex shader: %s\n", errorLog);
        glDeleteShader(vertex_shader);
        exit_fail(ctx);
    }

    printf("[info] init_egl: creating fragment shader\n");
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        glGetShaderInfoLog(fragment_shader, sizeof errorLog, NULL, errorLog);
        errorLog[strcspn(errorLog, "\n")] = '\0';
        printf("[error] init_egl: failed to compile fragment shader: %s\n", errorLog);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        exit_fail(ctx);
    }

    printf("[info] init_egl: creating shader program\n");
    ctx->egl->shader_program = glCreateProgram();
    glAttachShader(ctx->egl->shader_program, vertex_shader);
    glAttachShader(ctx->egl->shader_program, fragment_shader);
    glLinkProgram(ctx->egl->shader_program);
    glGetProgramiv(ctx->egl->shader_program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        printf("[error] init_egl: failed to link shader program\n");
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(ctx->egl->shader_program);
        exit_fail(ctx);
    }
    ctx->egl->invert_y_uniform = glGetUniformLocation(ctx->egl->shader_program, "uInvertY");
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    printf("[info] init_egl: redrawing frame\n");
    draw_texture_egl(ctx);

    printf("[info] init_egl: swapping buffers\n");
    if (eglSwapBuffers(ctx->egl->display, ctx->egl->surface) != EGL_TRUE) {
        printf("[error] init_egl: failed to swap buffers\n");
        exit_fail(ctx);
    }
}

// --- draw_texture_egl ---

void draw_texture_egl(ctx_t *ctx) {
    glClearColor(0.0, 0.0, 0.0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    if (ctx->egl->texture_initialized) {
        glUniform1i(ctx->egl->invert_y_uniform, ctx->egl->invert_y);
        glUseProgram(ctx->egl->shader_program);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), (void *)(0 * sizeof (float)));
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), (void *)(2 * sizeof (float)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, ctx->egl->vbo);
        glBindTexture(GL_TEXTURE_2D, ctx->egl->texture);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glFlush();
}

// --- resize_viewport_egl ---

void resize_viewport_egl(ctx_t * ctx) {
    uint32_t win_width = ctx->wl->scale * ctx->wl->width;
    uint32_t win_height = ctx->wl->scale * ctx->wl->height;
    uint32_t tex_width = ctx->egl->width;
    uint32_t tex_height = ctx->egl->height;
    uint32_t view_width = win_width;
    uint32_t view_height = win_height;

    float win_aspect = (float)win_width / win_height;
    float tex_aspect = (float)tex_width / tex_height;
    if (win_aspect > tex_aspect) {
        view_width = view_height * tex_aspect;
    } else if (win_aspect < tex_aspect) {
        view_height = view_width / tex_aspect;
    }

    printf("[info] resize_viewport_egl: resizing viewport\n");
    glViewport((win_width - view_width) / 2, (win_height - view_height) / 2, view_width, view_height);
}

// --- resize_window_egl ---

void resize_window_egl(ctx_t * ctx) {
    printf("[info] resize_window_egl: resizing EGL window\n");
    wl_egl_window_resize(ctx->egl->window, ctx->wl->scale * ctx->wl->width, ctx->wl->scale * ctx->wl->height, 0, 0);
    resize_viewport_egl(ctx);

    printf("[info] resize_window_egl: redrawing frame\n");
    draw_texture_egl(ctx);

    printf("[info] resize_window_egl: swapping buffers\n");
    if (eglSwapBuffers(ctx->egl->display, ctx->egl->surface) != EGL_TRUE) {
        printf("[error] configure_resize_egl: failed to swap buffers\n");
        exit_fail(ctx);
    }
}

// --- cleanup_egl ---

void cleanup_egl(ctx_t *ctx) {
    if (ctx->egl == NULL) return;

    printf("[info] cleanup_egl: destroying EGL objects\n");
    if (ctx->egl->shader_program != 0) glDeleteProgram(ctx->egl->shader_program);
    if (ctx->egl->texture != 0) glDeleteTextures(1, &ctx->egl->texture);
    if (ctx->egl->vbo != 0) glDeleteBuffers(1, &ctx->egl->vbo);
    if (ctx->egl->context != EGL_NO_CONTEXT) eglDestroyContext(ctx->egl->display, ctx->egl->context);
    if (ctx->egl->surface != EGL_NO_SURFACE) eglDestroySurface(ctx->egl->display, ctx->egl->surface);
    if (ctx->egl->window != EGL_NO_SURFACE) wl_egl_window_destroy(ctx->egl->window);
    if (ctx->egl->display != EGL_NO_DISPLAY) eglTerminate(ctx->egl->display);

    free(ctx->egl);
    ctx->egl = NULL;
}
