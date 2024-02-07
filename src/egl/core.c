#include <GLES2/gl2.h>
#include <wlm/context.h>

#define WLM_LOG_COMPONENT egl

// --- internal event handlers ---

void wlm_egl_core_on_window_changed(ctx_t * ctx) {
    if (!(ctx->wl.window.changed & WLM_WAYLAND_WINDOW_BUFFER_SIZE_CHANGED)) return;

    wlm_log(ctx, WLM_DEBUG, "resizing buffer to %dx%d", ctx->wl.window.buffer_width, ctx->wl.window.buffer_height);
    wl_egl_window_resize(ctx->egl.core.window, ctx->wl.window.buffer_width, ctx->wl.window.buffer_height, 0, 0);
}

// --- initialization and cleanup ---

void wlm_egl_core_zero(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "zeroing");

    ctx->egl.core.display = EGL_NO_DISPLAY;
    ctx->egl.core.config = NULL;
    ctx->egl.core.context = EGL_NO_CONTEXT;

    ctx->egl.core.window = NULL;
    ctx->egl.core.surface = EGL_NO_SURFACE;
}

void wlm_egl_core_init(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "initializing");

    // create egl display
    ctx->egl.core.display = eglGetDisplay((EGLNativeDisplayType)ctx->wl.core.display);
    if (ctx->egl.core.display == EGL_NO_DISPLAY) {
        wlm_log(ctx, WLM_FATAL, "failed to create EGL display");
        wlm_exit_fail(ctx);
    }

    // initialize egl display and check egl version
    EGLint major, minor;
    if (eglInitialize(ctx->egl.core.display, &major, &minor) != EGL_TRUE) {
        wlm_log(ctx, WLM_FATAL, "failed to initialize EGL display");
        wlm_exit_fail(ctx);
    }

    wlm_log(ctx, WLM_DEBUG, "initialized EGL %d.%d", major, minor);

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
        wlm_log(ctx, WLM_FATAL, "failed to get EGL config");
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
        wlm_log(ctx, WLM_FATAL, "failed to create EGL context");
        wlm_exit_fail(ctx);
    }

    // create wl_egl_window
    ctx->egl.core.window = wl_egl_window_create(ctx->wl.window.surface, ctx->wl.window.buffer_width, ctx->wl.window.buffer_height);

    // create egl surface
    ctx->egl.core.surface = eglCreateWindowSurface(ctx->egl.core.display, ctx->egl.core.config, (EGLNativeWindowType)ctx->egl.core.window, NULL);
    if (ctx->egl.core.surface == EGL_NO_SURFACE) {
        wlm_log(ctx, WLM_FATAL, "failed to create EGL surface");
        wlm_exit_fail(ctx);
    }

    // activate egl context
    if (eglMakeCurrent(ctx->egl.core.display, ctx->egl.core.surface, ctx->egl.core.surface, ctx->egl.core.context) != EGL_TRUE) {
        wlm_log(ctx, WLM_FATAL, "failed to activate EGL context");
        wlm_exit_fail(ctx);
    }
}

void wlm_egl_core_cleanup(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "cleaning up");

    if (ctx->egl.core.surface != EGL_NO_SURFACE) eglDestroySurface(ctx->egl.core.display, ctx->egl.core.surface);
    if (ctx->egl.core.window != NULL) wl_egl_window_destroy(ctx->egl.core.window);

    if (ctx->egl.core.context != EGL_NO_CONTEXT) eglDestroyContext(ctx->egl.core.display, ctx->egl.core.context);
    if (ctx->egl.core.display != EGL_NO_DISPLAY) eglTerminate(ctx->egl.core.display);

    wlm_wayland_core_zero(ctx);
}
