#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlm/context.h>
#include <EGL/eglext.h>
#include <wlm/mirror/backends.h>
#include <wlm/util.h>
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
    if (wlm_wayland_core_is_closing(ctx)) {
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
    wl_display_roundtrip(wlm_wayland_core_get_display(ctx));

    wlm_egl_draw_frame(ctx);

    (void)frame_callback;
    (void)msec;
}

static const struct wl_callback_listener frame_callback_listener = {
    .done = on_frame
};

// --- init_mirror ---

static wlm_fallback_backend_t auto_fallback_backends[];
void wlm_mirror_init(ctx_t * ctx) {
    // initialize context structure
    ctx->mirror.current_target = NULL;
    ctx->mirror.frame_callback = NULL;
    ctx->mirror.current_region = (region_t){ .x = 0, .y = 0, .width = 0, .height = 0 };
    ctx->mirror.invert_y = false;

    ctx->mirror.backend = NULL;
    ctx->mirror.fallback_backends = auto_fallback_backends;
    ctx->mirror.auto_backend_index = 0;

    ctx->mirror.initialized = true;

    // TODO: use wlm_mirror_target_parse
    // TODO: make wlm_mirror_target_parse implement finding output (or toplevel?) by region
    // TODO: only find and create target in a single place! (other place is opt parse)
    // finding target output
    wlm_wayland_output_entry_t * target_output = NULL;
    if (!wlm_opt_find_output(ctx, &target_output, &ctx->mirror.current_region)) {
        wlm_log_error("mirror::init(): failed to find output\n");
        wlm_exit_fail(ctx);
    }
    ctx->mirror.current_target = wlm_mirror_target_create_output(ctx, target_output);

    // update window title
    wlm_mirror_update_title(ctx);

    // add frame callback listener
    ctx->mirror.frame_callback = wl_surface_frame(ctx->wl.surface);
    wl_callback_add_listener(ctx->mirror.frame_callback, &frame_callback_listener, (void *)ctx);
}

// --- auto backend handler

static wlm_fallback_backend_t auto_fallback_backends[] = {
#ifdef WITH_GBM
    { "extcopy-dmabuf", wlm_mirror_extcopy_dmabuf_init },
    { "screencopy-dmabuf", wlm_mirror_screencopy_dmabuf_init },
#endif
    { "export-dmabuf", wlm_mirror_export_dmabuf_init },
    { "extcopy-shm", wlm_mirror_extcopy_shm_init },
    { "screencopy-shm", wlm_mirror_screencopy_shm_init },
    { NULL, NULL }
};

static wlm_fallback_backend_t auto_screencopy_backends[] = {
#ifdef WITH_GBM
    { "screencopy-dmabuf", wlm_mirror_screencopy_dmabuf_init },
#endif
    { "screencopy-shm", wlm_mirror_screencopy_shm_init },
    { NULL, NULL }
};

static wlm_fallback_backend_t auto_extcopy_backends[] = {
#ifdef WITH_GBM
    { "extcopy-dmabuf", wlm_mirror_extcopy_dmabuf_init },
#endif
    { "extcopy-shm", wlm_mirror_extcopy_shm_init },
    { NULL, NULL }
};

static void auto_backend_fallback(ctx_t * ctx) {
    while (true) {
        // get next backend
        size_t index = ctx->mirror.auto_backend_index;
        wlm_fallback_backend_t * next_backend = &ctx->mirror.fallback_backends[index];
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
            ctx->mirror.fallback_backends = auto_fallback_backends;
            ctx->mirror.auto_backend_index = 0;
            auto_backend_fallback(ctx);
            break;

        case BACKEND_EXPORT_DMABUF:
            wlm_mirror_export_dmabuf_init(ctx);
            break;

        case BACKEND_SCREENCOPY_AUTO:
            ctx->mirror.fallback_backends = auto_screencopy_backends;
            ctx->mirror.auto_backend_index = 0;
            auto_backend_fallback(ctx);
            break;

        case BACKEND_SCREENCOPY_DMABUF:
            wlm_mirror_screencopy_dmabuf_init(ctx);
            break;

        case BACKEND_SCREENCOPY_SHM:
            wlm_mirror_screencopy_shm_init(ctx);
            break;

        case BACKEND_EXTCOPY_AUTO:
            ctx->mirror.fallback_backends = auto_extcopy_backends;
            ctx->mirror.auto_backend_index = 0;
            auto_backend_fallback(ctx);
            break;

        case BACKEND_EXTCOPY_DMABUF:
            wlm_mirror_extcopy_dmabuf_init(ctx);
            break;

        case BACKEND_EXTCOPY_SHM:
            wlm_mirror_extcopy_shm_init(ctx);
            break;
    }

    if (ctx->mirror.backend == NULL) wlm_exit_fail(ctx);
}

// --- output_removed ---

void wlm_mirror_output_removed(ctx_t * ctx, wlm_wayland_output_entry_t * node) {
    if (!ctx->mirror.initialized) return;
    if (ctx->mirror.current_target == NULL) return;
    if (wlm_mirror_target_get_output_node(ctx->mirror.current_target) != node) return;

    wlm_log_error("mirror::output_removed(): output disappeared, closing\n");
    wlm_exit_fail(ctx);
}

// --- update_title ---

typedef struct {
    const char *specifier;
    char type;
    union { int d; const char *s; };
} specifier_t;

static size_t specifier_str(ctx_t * ctx, char *dst, int n, specifier_t specifier) {
    switch (specifier.type) {
        case 'd':
            return snprintf(dst, n, "%d", specifier.d);
        case 's':
            return snprintf(dst, n, "%s", specifier.s);
        default:
            wlm_log_error("mirror::specifier_str(): unrecognized format type '%d'", specifier.type);
            wlm_exit_fail(ctx);
    }
}

static int format_title(ctx_t * ctx, char ** dst, char * fmt) {
    // guard against mirror not being initialized
    // for initial setting of the window title
    int x                    = !ctx->mirror.initialized ? 0 : ctx->mirror.current_region.x;
    int y                    = !ctx->mirror.initialized ? 0 : ctx->mirror.current_region.y;
    int width                = !ctx->mirror.initialized ? 0 : ctx->mirror.current_region.width;
    int height               = !ctx->mirror.initialized ? 0 : ctx->mirror.current_region.height;

    uint32_t target_width = 0;
    uint32_t target_height = 0;
    const char * target_name = "";

    if (ctx->mirror.initialized && ctx->mirror.current_target != NULL) {
        // TODO: support for other target types
        wlm_wayland_output_entry_t * output_node = wlm_mirror_target_get_output_node(ctx->mirror.current_target);
        if (output_node != NULL) {
            target_width = output_node->width;
            target_height = output_node->height;
            target_name = output_node->name;
        }
    }

    specifier_t replacements[] = {
        {"{x}", 'd', {.d = x}},
        {"{y}", 'd', {.d = y}},
        {"{width}", 'd', {.d = width}},
        {"{height}", 'd', {.d = height}},
        {"{target_width}", 'd', {.d = target_width}},
        {"{target_height}", 'd', {.d = target_height}},
        {"{target_output}", 's', {.s = target_name}}
    };

    size_t length = strlen(fmt);
    for (size_t f = 0; f < ARRAY_LENGTH(replacements); f++) {
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
        for (size_t f = 0; f < ARRAY_LENGTH(replacements); f++) {
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

void wlm_mirror_options_updated(ctx_t * ctx) {
    if (ctx->mirror.backend != NULL && ctx->mirror.backend->on_options_updated != NULL) {
        ctx->mirror.backend->on_options_updated(ctx);
    }
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

    if (ctx->mirror.current_target != NULL) wlm_mirror_target_destroy(ctx->mirror.current_target);
    if (ctx->mirror.backend != NULL) ctx->mirror.backend->do_cleanup(ctx);
    if (ctx->mirror.frame_callback != NULL) wl_callback_destroy(ctx->mirror.frame_callback);

    ctx->mirror.initialized = false;
}
