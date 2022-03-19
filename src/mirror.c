#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "context.h"
#include <EGL/eglext.h>
#include "linux-dmabuf-unstable-v1.h"
#include "mirror-backends.h"

// --- frame_callback event handlers ---

static const struct wl_callback_listener frame_callback_listener;

static void frame_callback_event_done(
    void * data, struct wl_callback * frame_callback, uint32_t msec
) {
    ctx_t * ctx = (ctx_t *)data;

    wl_callback_destroy(ctx->mirror.frame_callback);
    ctx->mirror.frame_callback = NULL;

    log_debug(ctx, "frame_callback: requesting next callback\n");
    ctx->mirror.frame_callback = wl_surface_frame(ctx->wl.surface);
    wl_callback_add_listener(ctx->mirror.frame_callback, &frame_callback_listener, (void *)ctx);

    log_debug(ctx, "frame_callback: rendering frame\n");
    draw_texture_egl(ctx);

    log_debug(ctx, "frame_callback: swapping buffers\n");
    eglSwapInterval(ctx->egl.display, 0);
    if (eglSwapBuffers(ctx->egl.display, ctx->egl.surface) != EGL_TRUE) {
        log_error("frame_callback: failed to swap buffers\n");
        exit_fail(ctx);
    }

    if (ctx->mirror.backend != NULL) {
        if (ctx->mirror.backend->fail_count >= MIRROR_BACKEND_FATAL_FAILCOUNT) {
            backend_fail(ctx);
        }

        ctx->mirror.backend->on_frame(ctx);
    }

    (void)frame_callback;
    (void)msec;
}

static const struct wl_callback_listener frame_callback_listener = {
    .done = frame_callback_event_done
};

// --- init_mirror ---

void init_mirror(ctx_t * ctx) {
    log_debug(ctx, "init_mirror: initializing context structure\n");

    ctx->mirror.current_target = NULL;
    ctx->mirror.frame_callback = NULL;
    ctx->mirror.current_region = (region_t){ .x = 0, .y = 0, .width = 0, .height = 0 };
    ctx->mirror.invert_y = false;

    ctx->mirror.backend = NULL;
    ctx->mirror.auto_backend_index = 0;

    ctx->mirror.frame_image = EGL_NO_IMAGE;

    ctx->mirror.initialized = true;

    if (!find_output_opt(ctx, &ctx->mirror.current_target, &ctx->mirror.current_region)) {
        log_error("init_mirror: failed to find output\n");
        exit_fail(ctx);
    }

    update_options_mirror(ctx);

    log_debug(ctx, "init_mirror: requesting render callback\n");
    ctx->mirror.frame_callback = wl_surface_frame(ctx->wl.surface);
    wl_callback_add_listener(ctx->mirror.frame_callback, &frame_callback_listener, (void *)ctx);
}

// --- auto backend handler

typedef struct {
    char * name;
    void (*init)(ctx_t * ctx);
} fallback_backend_t;

static fallback_backend_t auto_fallback_backends[] = {
    { "dmabuf", init_mirror_dmabuf },
    { "screencopy", init_mirror_screencopy },
    { NULL, NULL }
};

static void auto_backend_fallback(ctx_t * ctx) {
    while (true) {
        size_t index = ctx->mirror.auto_backend_index;
        fallback_backend_t * next_backend = &auto_fallback_backends[index];

        if (next_backend->name == NULL) {
            log_error("auto_backend_fallback: no working backend found, exiting\n");
            exit_fail(ctx);
        }

        if (index > 0) {
            log_warn("auto_backend_fallback: falling back to backend %s\n", next_backend->name);
        } else {
            log_debug(ctx, "auto_backend_fallback: selecting backend %s\n", next_backend->name);
        }

        if (ctx->mirror.backend != NULL) ctx->mirror.backend->on_cleanup(ctx);
        next_backend->init(ctx);
        if (ctx->mirror.backend != NULL) break;

        ctx->mirror.auto_backend_index++;
    }
}


// --- init_mirror_backend ---

void init_mirror_backend(ctx_t * ctx) {
    if (ctx->mirror.backend != NULL) ctx->mirror.backend->on_cleanup(ctx);

    switch (ctx->opt.backend) {
        case BACKEND_AUTO:
            auto_backend_fallback(ctx);
            break;

        case BACKEND_DMABUF:
            init_mirror_dmabuf(ctx);
            break;

        case BACKEND_SCREENCOPY:
            init_mirror_screencopy(ctx);
            break;
    }

    if (ctx->mirror.backend == NULL) exit_fail(ctx);
}

// --- output_removed_mirror ---

void output_removed_mirror(ctx_t * ctx, output_list_node_t * node) {
    if (!ctx->mirror.initialized) return;
    if (ctx->mirror.current_target == NULL) return;
    if (ctx->mirror.current_target != node) return;

    log_error("output_removed_mirror: output disappeared, closing\n");
    exit_fail(ctx);
}

// --- update_options_mirror ---

void update_options_mirror(ctx_t * ctx) {
    log_debug(ctx, "init_mirror: formatting window title\n");
    char * title = NULL;
    int status = asprintf(&title, "Wayland Output Mirror for %s", ctx->mirror.current_target->name);
    if (status == -1) {
        log_error("init_mirror: failed to format window title\n");
        exit_fail(ctx);
    }

    log_debug(ctx, "init_mirror: setting window title\n");
    xdg_toplevel_set_title(ctx->wl.xdg_toplevel, title);
    free(title);
}

// --- backend_fail ---

void backend_fail(ctx_t * ctx) {
    if (ctx->opt.backend == BACKEND_AUTO) {
        auto_backend_fallback(ctx);
    } else {
        exit_fail(ctx);
    }
}

// --- cleanup_mirror ---

void cleanup_mirror(ctx_t * ctx) {
    if (!ctx->mirror.initialized) return;

    log_debug(ctx, "cleanup_mirror: destroying mirror objects\n");

    if (ctx->mirror.backend != NULL) ctx->mirror.backend->on_cleanup(ctx);
    if (ctx->mirror.frame_callback != NULL) wl_callback_destroy(ctx->mirror.frame_callback);
    if (ctx->mirror.frame_image != EGL_NO_IMAGE) eglDestroyImage(ctx->egl.display, ctx->mirror.frame_image);

    ctx->mirror.initialized = false;
}
