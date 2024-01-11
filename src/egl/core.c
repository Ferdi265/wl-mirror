#include <GLES2/gl2.h>
#include <wlm/context.h>

// --- initialization and cleanup ---

void wlm_egl_core_zero(ctx_t * ctx) {
    ctx->egl.core.display = EGL_NO_DISPLAY;
    ctx->egl.core.config = NULL;
    ctx->egl.core.context = EGL_NO_CONTEXT;

    ctx->egl.core.window = NULL;
    ctx->egl.core.surface = EGL_NO_SURFACE;
}

void wlm_egl_core_init(ctx_t * ctx) {
    (void)ctx;
}

void wlm_egl_core_cleanup(ctx_t * ctx) {
    if (ctx->egl.core.surface != EGL_NO_SURFACE) eglDestroySurface(ctx->egl.core.display, ctx->egl.core.surface);
    if (ctx->egl.core.window != NULL) wl_egl_window_destroy(ctx->egl.core.window);

    if (ctx->egl.core.context != EGL_NO_CONTEXT) eglDestroyContext(ctx->egl.core.display, ctx->egl.core.context);
    if (ctx->egl.core.display != EGL_NO_DISPLAY) eglTerminate(ctx->egl.core.display);

    wlm_wayland_core_zero(ctx);
}

// --- internal events ---

void wlm_egl_core_on_window_initial_configure(ctx_t * ctx) {
    // create egl display
    ctx->egl.core.display = eglGetDisplay((EGLNativeDisplayType)ctx->wl.core.display);
    if (ctx->egl.core.display == EGL_NO_DISPLAY) {
        log_error("egl::core::on_window_initial_configure(): failed to create EGL display\n");
        wlm_exit_fail(ctx);
    }

    // initialize egl display and check egl version
    EGLint major, minor;
    if (eglInitialize(ctx->egl.core.display, &major, &minor) != EGL_TRUE) {
        log_error("egl::core::on_window_initial_configure(): failed to initialize EGL display\n");
        wlm_exit_fail(ctx);
    }

    log_debug(ctx, "egl::core::on_window_initial_configure(): initialized EGL %d.%d\n", major, minor);

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
    if (eglChooseConfig(ctx->egl.core.display, config_attribs, &ctx->egl.core.config, 1, &num_configs) != EGL_TRUE) {
        log_error("egl::core::on_window_initial_configure(): failed to get EGL config\n");
        wlm_exit_fail(ctx);
    }

    // create egl context with support for OpenGL ES 2.0
    EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 2,
        EGL_CONTEXT_MINOR_VERSION, 0,
        EGL_NONE
    };
    ctx->egl.core.context = eglCreateContext(ctx->egl.core.display, ctx->egl.core.config, EGL_NO_CONTEXT, context_attribs);
    if (ctx->egl.core.context == EGL_NO_CONTEXT) {
        log_error("egl::core::on_window_initial_configure(): failed to create EGL context\n");
        wlm_exit_fail(ctx);
    }

    // create wl_egl_window
    ctx->egl.core.window = wl_egl_window_create(ctx->wl.window.surface, ctx->wl.window.buffer_width, ctx->wl.window.buffer_height);

    // create egl surface
    ctx->egl.core.surface = eglCreateWindowSurface(ctx->egl.core.display, ctx->egl.core.config, (EGLNativeWindowType)ctx->egl.core.window, NULL);
    if (ctx->egl.core.surface == EGL_NO_SURFACE) {
        log_error("egl::core::on_window_initial_configure(): failed to create EGL surface\n");
        wlm_exit_fail(ctx);
    }

    // activate egl context
    if (eglMakeCurrent(ctx->egl.core.display, ctx->egl.core.surface, ctx->egl.core.surface, ctx->egl.core.context) != EGL_TRUE) {
        log_error("egl::core::on_window_initial_configure(): failed to activate EGL context\n");
        wlm_exit_fail(ctx);
    }

    // TODO: trigger redraw?
    glClear(GL_COLOR_BUFFER_BIT);
    if (eglSwapBuffers(ctx->egl.core.display, ctx->egl.core.surface) != EGL_TRUE) {
        log_error("egl::core::on_window_initial_configure(): failed to swap buffers\n");
        wlm_exit_fail(ctx);
    }
}


void wlm_egl_core_on_window_changed(ctx_t * ctx) {
    if (!(ctx->wl.window.changed & WLM_WAYLAND_WINDOW_BUFFER_SIZE_CHANGED)) return;

    log_debug(ctx, "egl::core::on_window_changed(): resizing buffer to %dx%d\n", ctx->wl.window.buffer_width, ctx->wl.window.buffer_height);
    wl_egl_window_resize(ctx->egl.core.window, ctx->wl.window.buffer_width, ctx->wl.window.buffer_height, 0, 0);

    // TODO: trigger redraw?
}
