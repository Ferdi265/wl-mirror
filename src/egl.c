#include <stdio.h>
#include <stdlib.h>
#include "context.h"
#include <GLES2/gl2.h>

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

    printf("[info] init_egl: clearing frame\n");
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();

    printf("[info] init_egl: swapping buffers\n");
    if (eglSwapBuffers(ctx->egl->display, ctx->egl->surface) != EGL_TRUE) {
        printf("[error] init_egl: failed to swap buffers\n");
        exit_fail(ctx);
    }
}

// --- configure_resize_egl ---

void configure_resize_egl(ctx_t * ctx, uint32_t width, uint32_t height) {
    printf("[info] configure_resize_egl: resizing EGL window\n");
    wl_egl_window_resize(ctx->egl->window, width, height, 0, 0);

    printf("[info] configure_resize_egl: clearing frame\n");
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();

    printf("[info] configure_resize_egl: swapping buffers\n");
    if (eglSwapBuffers(ctx->egl->display, ctx->egl->surface) != EGL_TRUE) {
        printf("[error] configure_resize_egl: failed to swap buffers\n");
        exit_fail(ctx);
    }
}

// --- cleanup_egl ---

void cleanup_egl(ctx_t *ctx) {
    if (ctx->egl == NULL) return;

    printf("[info] cleanup_egl: destroying EGL objects\n");
    if (ctx->egl->context != EGL_NO_CONTEXT) eglDestroyContext(ctx->egl->display, ctx->egl->context);
    if (ctx->egl->surface != EGL_NO_SURFACE) eglDestroySurface(ctx->egl->display, ctx->egl->surface);
    if (ctx->egl->window != EGL_NO_SURFACE) wl_egl_window_destroy(ctx->egl->window);
    if (ctx->egl->display != EGL_NO_DISPLAY) eglTerminate(ctx->egl->display);

    free(ctx->egl);
    ctx->egl = NULL;
}

