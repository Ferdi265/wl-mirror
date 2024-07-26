#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlm/context.h>
#include <EGL/eglext.h>
#include <wlm/mirror-backends.h>
#include <wlm/proto/linux-dmabuf-unstable-v1.h>

// --- frame_callback event handlers ---

static const struct wl_callback_listener frame_callback_listener;

static void on_frame(
    void * data, struct wl_callback * frame_callback, uint32_t msec
) {
    ctx_t * ctx = (ctx_t *)data;

    // destroy frame callback
    wl_callback_destroy(ctx->mirror.frame_callback);
    ctx->mirror.frame_callback = NULL;

    // don't attempt to render if window is already closing
    if (ctx->wl.closing) {
        return;
    }

    // add new frame callback listener
    // the wayland spec says you cannot reuse the old frame callback
    ctx->mirror.frame_callback = wl_surface_frame(ctx->wl.surface);
    wl_callback_add_listener(ctx->mirror.frame_callback, &frame_callback_listener, (void *)ctx);

    if (ctx->mirror.backend != NULL) {
        // check if backend failure count exceeded
        if (ctx->mirror.backend->fail_count >= MIRROR_BACKEND_FATAL_FAILCOUNT) {
            wlm_mirror_backend_fail(ctx);
        }

        if (!ctx->opt.freeze) {
            // request new screen capture from backend
            ctx->mirror.backend->do_capture(ctx);
        }
    }

    // wait for events
    // - screencapture events from backend
    wl_display_roundtrip(ctx->wl.display);

    // render frame, set swap interval to 0 to ensure nonblocking buffer swap
    wlm_egl_draw_texture(ctx);
    eglSwapInterval(ctx->egl.display, 0);
    if (eglSwapBuffers(ctx->egl.display, ctx->egl.surface) != EGL_TRUE) {
        wlm_log_error("mirror::on_frame(): failed to swap buffers\n");
        wlm_exit_fail(ctx);
    }

    (void)frame_callback;
    (void)msec;
}

static const struct wl_callback_listener frame_callback_listener = {
    .done = on_frame
};

// --- init_mirror ---

void wlm_mirror_init(ctx_t * ctx) {
    // initialize context structure
    ctx->mirror.current_target = NULL;
    ctx->mirror.frame_callback = NULL;
    ctx->mirror.current_region = (region_t){ .x = 0, .y = 0, .width = 0, .height = 0 };
    ctx->mirror.invert_y = false;

    ctx->mirror.backend = NULL;
    ctx->mirror.auto_backend_index = 0;

    ctx->mirror.initialized = true;

    // finding target output
    if (!wlm_opt_find_output(ctx, &ctx->mirror.current_target, &ctx->mirror.current_region)) {
        wlm_log_error("mirror::init(): failed to find output\n");
        wlm_exit_fail(ctx);
    }

    // update window title
    wlm_mirror_update_title(ctx);

    // add frame callback listener
    ctx->mirror.frame_callback = wl_surface_frame(ctx->wl.surface);
    wl_callback_add_listener(ctx->mirror.frame_callback, &frame_callback_listener, (void *)ctx);
}

// --- auto backend handler

typedef struct {
    char * name;
    void (*init)(ctx_t * ctx);
} fallback_backend_t;

static fallback_backend_t auto_fallback_backends[] = {
    { "dmabuf", wlm_mirror_dmabuf_init },
    { "screencopy", wlm_mirror_screencopy_init },
    { NULL, NULL }
};

static void auto_backend_fallback(ctx_t * ctx) {
    while (true) {
        // get next backend
        size_t index = ctx->mirror.auto_backend_index;
        fallback_backend_t * next_backend = &auto_fallback_backends[index];
        if (next_backend->name == NULL) {
            wlm_log_error("mirror::auto_backend_fallback(): no working backend found, exiting\n");
            wlm_exit_fail(ctx);
        }

        if (index > 0) {
            wlm_log_warn("mirror::auto_backend_fallback(): falling back to backend %s\n", next_backend->name);
        } else {
            wlm_log_debug(ctx, "mirror::auto_backend_fallback(): selecting backend %s\n", next_backend->name);
        }

        // uninitialize previous backend
        if (ctx->mirror.backend != NULL) ctx->mirror.backend->do_cleanup(ctx);

        // initialize next backend
        next_backend->init(ctx);

        // increment backend index for next attempt
        ctx->mirror.auto_backend_index++;

        // break if backend loading succeeded
        if (ctx->mirror.backend != NULL) break;
    }
}


// --- init_mirror_backend ---

void wlm_mirror_backend_init(ctx_t * ctx) {
    if (ctx->mirror.backend != NULL) ctx->mirror.backend->do_cleanup(ctx);

    switch (ctx->opt.backend) {
        case BACKEND_AUTO:
            auto_backend_fallback(ctx);
            break;

        case BACKEND_DMABUF:
            wlm_mirror_dmabuf_init(ctx);
            break;

        case BACKEND_SCREENCOPY:
            wlm_mirror_screencopy_init(ctx);
            break;
    }

    if (ctx->mirror.backend == NULL) wlm_exit_fail(ctx);
}

// --- output_removed ---

void wlm_mirror_output_removed(ctx_t * ctx, output_list_node_t * node) {
    if (!ctx->mirror.initialized) return;
    if (ctx->mirror.current_target == NULL) return;
    if (ctx->mirror.current_target != node) return;

    wlm_log_error("mirror::output_removed(): output disappeared, closing\n");
    wlm_exit_fail(ctx);
}

// --- update_options_mirror ---

typedef struct {
    const char *specifier;
    const char *format;
    union { int d; const char *s; };
} specifier_t;

static size_t specifier_str(ctx_t * ctx, char *dst, int n, specifier_t specifier) {
    switch (specifier.format[1]) {
        case 'd':
            return snprintf(dst, n, specifier.format, specifier.d);
        case 's':
            return snprintf(dst, n, specifier.format, specifier.s);
        default:
            wlm_log_error("mirror::specifier_str(): unrecognized format %s", specifier.format);
            wlm_exit_fail(ctx);
    }
}

static int format_title(ctx_t * ctx, char ** dst, char * fmt) {
    specifier_t replacements[] = {
        {"{x}", "%d", {.d = ctx->mirror.current_region.x}},
        {"{y}", "%d", {.d = ctx->mirror.current_region.y}},
        {"{width}", "%d", {.d = ctx->mirror.current_region.width}},
        {"{height}", "%d", {.d = ctx->mirror.current_region.height}},
        {"{target_width}", "%d", {.d = ctx->mirror.current_target->width}},
        {"{target_height}", "%d", {.d = ctx->mirror.current_target->height}},
        {"{target_output}", "%s", {.s = ctx->mirror.current_target->name}}
    };
    size_t n_specifiers = sizeof(replacements)/sizeof(replacements[0]);

    size_t length = strlen(fmt);
    for (size_t f = 0; f < n_specifiers; f++) {
        const char *fmt_cursor = fmt;
        size_t spec_len = strlen(replacements[f].specifier);

        while ((fmt_cursor = strstr(fmt_cursor, replacements[f].specifier))) {
            length += specifier_str(ctx, NULL, 0, replacements[f]) - spec_len;
            fmt_cursor += spec_len;
        }
    }

    *dst = (char *) calloc(++length, sizeof(char));
    if (*dst == NULL) {
        wlm_log_error("mirror::format_title(): failed to allocate memory\n");
        wlm_exit_fail(ctx);
    }

    char *dst_cursor = *dst;
    const char *fmt_cursor = fmt;
    while (*fmt_cursor) {
        bool replaced = false;
        for (size_t f = 0; f < n_specifiers; f++) {
            size_t spec_len = strlen(replacements[f].specifier);
            if (strncmp(fmt_cursor, replacements[f].specifier, spec_len) == 0) {
                size_t written = specifier_str(ctx, dst_cursor, length, replacements[f]);
                dst_cursor += written;
                fmt_cursor += spec_len;
                length -= written;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            *dst_cursor++ = *fmt_cursor++;
            length--;
        }
    }
    *dst_cursor = '\0';


    return 0;
}

void wlm_mirror_update_title(ctx_t * ctx) {
    char * title = NULL;
    char * title_fmt = "Wayland Output Mirror for {target_output}";
    if (ctx->opt.window_title != NULL) {
        title_fmt = ctx->opt.window_title;
    }

    int status = format_title(ctx, &title, title_fmt);
    if (status == -1) {
        wlm_log_error("mirror::update_title(): failed to format window title\n");
        wlm_exit_fail(ctx);
    }

    wlm_wayland_window_set_title(ctx, title);
    free(title);
}

// --- backend_fail ---

void wlm_mirror_backend_fail(ctx_t * ctx) {
    if (ctx->opt.backend == BACKEND_AUTO) {
        auto_backend_fallback(ctx);
    } else {
        wlm_exit_fail(ctx);
    }
}

// --- cleanup_mirror ---

void wlm_mirror_cleanup(ctx_t * ctx) {
    if (!ctx->mirror.initialized) return;

    wlm_log_debug(ctx, "mirror::cleanup(): destroying mirror objects\n");

    if (ctx->mirror.backend != NULL) ctx->mirror.backend->do_cleanup(ctx);
    if (ctx->mirror.frame_callback != NULL) wl_callback_destroy(ctx->mirror.frame_callback);

    ctx->mirror.initialized = false;
}
