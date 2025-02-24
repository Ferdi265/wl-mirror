#include <wlm/mirror-extcopy.h>
#include <wlm/context.h>
#include <wlm/wayland/shm.h>
#include <wlm/wayland/dmabuf.h>
#include <wlm/egl/shm.h>
#include <wlm/egl/dmabuf.h>
#include <wlm/egl/formats.h>
#include <stdlib.h>
#include <string.h>

static void extcopy_frame_cleanup(extcopy_mirror_backend_t * backend) {
    if (backend->capture_frame != NULL) ext_image_copy_capture_frame_v1_destroy(backend->capture_frame);
    if (backend->frame_drm_modifiers != NULL) free(backend->frame_drm_modifiers);

    backend->capture_frame = NULL;
    backend->has_shm_format = false;
    backend->has_drm_format = false;
    backend->frame_width = 0;
    backend->frame_height = 0;
    backend->frame_shm_stride = 0;
    backend->frame_shm_format = 0;
    backend->frame_drm_format = 0;
    backend->frame_drm_modifiers = NULL;
    backend->frame_num_drm_modifiers = 0;
}

static void extcopy_session_cleanup(ctx_t * ctx, extcopy_mirror_backend_t * backend) {
    extcopy_frame_cleanup(backend);

    if (backend->capture_session != NULL) ext_image_copy_capture_session_v1_destroy(backend->capture_session);
    if (backend->capture_source != NULL) ext_image_capture_source_v1_destroy(backend->capture_source);
    if (wlm_wayland_dmabuf_get_buffer(ctx) != NULL) wlm_wayland_dmabuf_dealloc(ctx);
    if (wlm_wayland_shm_get_buffer(ctx) != NULL) wlm_wayland_shm_dealloc(ctx);

    backend->capture_session = NULL;
    backend->capture_source = NULL;
}

static void backend_cancel(ctx_t * ctx, extcopy_mirror_backend_t * backend) {
    wlm_log_error("mirror-extcopy::backend_cancel(): cancelling capture due to error\n");

    extcopy_session_cleanup(ctx, backend);

    backend->state = STATE_CANCELED;
    backend->header.fail_count++;
}

// --- capture session event handlers ---

static void on_capture_session_buffer_size(void * data, struct ext_image_copy_capture_session_v1 * session, uint32_t width, uint32_t height) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_buffer_size(): size = %dx%d\n", width, height);
    backend->frame_width = width;
    backend->frame_height = height;

    (void)session;
}

static void on_capture_session_shm_format(void * data, struct ext_image_copy_capture_session_v1 * session, uint32_t shm_format) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    if (backend->use_dmabuf) return;

    if (!backend->has_shm_format) {
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_shm_format(): shm_format = %x\n", shm_format);
        backend->has_shm_format = true;
        backend->frame_shm_format = shm_format;

        const wlm_egl_format_t * format = wlm_egl_formats_find_shm(backend->frame_shm_format);
        if (format == NULL) {
            wlm_log_error("mirror-extcopy::on_capture_session_shm_format(): failed to find bpp for shm format\n");
            backend_cancel(ctx, backend);
            return;
        }

        backend->frame_shm_stride = backend->frame_width * format->bpp;
    } else {
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_shm_format(): alternate shm_format = %x (ignoring)\n", shm_format);
        return;
    }

    (void)session;
}

static void on_capture_session_dmabuf_format(void * data, struct ext_image_copy_capture_session_v1 * session, uint32_t drm_format, struct wl_array * modifiers_arr) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    if (!backend->use_dmabuf) return;

    if (!backend->has_drm_format) {
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_dmabuf_format(): drm_format = %x\n", drm_format);
        backend->has_drm_format = true;
        backend->frame_drm_format = drm_format;
    } else {
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_dmabuf_format(): alternate drm_format = %x (ignoring)\n", drm_format);
        return;
    }

    if (modifiers_arr->size % sizeof (uint64_t) != 0) {
        wlm_log_error("mirror-extcopy::on_capture_session_dmabuf_format(): modifiers have invalid size %zd, expected multiple of %zd\n", modifiers_arr->size, sizeof (uint64_t));
        backend_cancel(ctx, backend);
        return;
    }

    if (backend->frame_drm_modifiers != NULL) {
        free(backend->frame_drm_modifiers);
        backend->frame_drm_modifiers = NULL;
    }

    backend->frame_num_drm_modifiers = modifiers_arr->size / sizeof (uint64_t);
    backend->frame_drm_modifiers = (uint64_t *)calloc(backend->frame_num_drm_modifiers, sizeof (uint64_t));
    if (backend->frame_drm_modifiers == NULL) {
        wlm_log_error("mirror-extcopy::on_capture_session_dmabuf_format(): failed to allocate modifiers array\n");
        backend_cancel(ctx, backend);
        return;
    }

    memcpy(backend->frame_drm_modifiers, modifiers_arr->data, modifiers_arr->size);

    (void)session;
}

static void on_capture_session_dmabuf_device(void * data, struct ext_image_copy_capture_session_v1 * session, struct wl_array * device) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    if (!backend->use_dmabuf) return;

    wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_dmabuf_device(): opening device\n");
    if (device->size != sizeof (dev_t)) {
        wlm_log_error("mirror-extcopy::on_capture_session_dmabuf_device(): array size mismatch: %zd != %zd\n", device->size, sizeof (dev_t));
        backend_cancel(ctx, backend);
        return;
    }

    dev_t device_id;
    memcpy(&device_id, device->data, sizeof (dev_t));

    if (!wlm_wayland_dmabuf_open_device(ctx, device_id)) {
        wlm_log_error("mirror-extcopy::on_capture_session_dmabuf_device(): failed to open device\n");
        backend_cancel(ctx, backend);
        return;
    }

    (void)session;
}

static void on_dmabuf_allocated(ctx_t * ctx, bool success);
static void on_capture_session_done(void * data, struct ext_image_copy_capture_session_v1 * session) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    if (backend->capture_frame != NULL) {
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_done(): destroying old capture frame\n");
        ext_image_copy_capture_frame_v1_destroy(backend->capture_frame);
        backend->capture_frame = NULL;
    }

    if (backend->use_dmabuf) {
        if (!backend->has_drm_format) {
            wlm_log_error("mirror-extcopy::on_capture_session_done(): missing DRM format\n");
            backend_cancel(ctx, backend);
            return;
        }

        if (backend->frame_drm_modifiers == NULL) {
            wlm_log_error("mirror-extcopy::on_capture_session_done(): missing DRM modifiers\n");
            backend_cancel(ctx, backend);
            return;
        }

        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_done(): allocating DMA-BUF\n");
        wlm_wayland_dmabuf_dealloc(ctx);
        wlm_wayland_dmabuf_alloc(ctx, backend->frame_drm_format, backend->frame_width, backend->frame_height, backend->frame_drm_modifiers, backend->frame_num_drm_modifiers, on_dmabuf_allocated);
    } else {
        if (!backend->has_shm_format) {
            wlm_log_error("mirror-extcopy::on_capture_session_done(): missing SHM format\n");
            backend_cancel(ctx, backend);
            return;
        }

        wlm_wayland_shm_dealloc(ctx);
        bool success = wlm_wayland_shm_alloc(ctx, backend->frame_shm_format, backend->frame_width, backend->frame_height, backend->frame_shm_stride);

        if (!success) {
            wlm_log_error("mirror-extcopy::on_capture_session_done(): failed to allocate SHM buffer\n");
            backend_cancel(ctx, backend);
            return;
        }

        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_done(): SHM buffer allocated\n");
        backend->state = STATE_READY;
    }

    (void)session;
}

static void on_capture_session_stopped(void * data, struct ext_image_copy_capture_session_v1 * session) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_error("mirror-extcopy::on_capture_session_stoppped(): capture session closed unexpectedly\n");
    backend_cancel(ctx, backend);

    (void)session;
}

static void on_dmabuf_allocated(ctx_t * ctx, bool success) {
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    if (!success) {
        wlm_log_error("mirror-extcopy::on_dmabuf_allocated(): failed to allocate DMA-BUF\n");
        backend_cancel(ctx, backend);
        return;
    }

    wlm_log_debug(ctx, "mirror-extcopy::on_dmabuf_allocated(): DMA-BUF allocated\n");
    backend->state = STATE_READY;
}

static const struct ext_image_copy_capture_session_v1_listener capture_session_listener = {
    .buffer_size = on_capture_session_buffer_size,
    .shm_format = on_capture_session_shm_format,
    .dmabuf_format = on_capture_session_dmabuf_format,
    .dmabuf_device = on_capture_session_dmabuf_device,
    .done = on_capture_session_done,
    .stopped = on_capture_session_stopped,
};

// --- capture frame event handlers ---

static void on_capture_frame_transform(void * data, struct ext_image_copy_capture_frame_v1 * frame, uint32_t transform) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_debug(ctx, "mirror-extcopy::on_capture_frame_transform(): transform = %d\n", transform);
    (void)frame;
    (void)backend;
}

static void on_capture_frame_damage(void * data, struct ext_image_copy_capture_frame_v1 * frame,
    int32_t x, int32_t y, int32_t width, int32_t height
) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    (void)ctx;
    (void)backend;
    (void)frame;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

static void on_capture_frame_presentation_time(void * data, struct ext_image_copy_capture_frame_v1 * frame,
    uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec
) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    (void)ctx;
    (void)backend;
    (void)frame;
    (void)tv_sec_hi;
    (void)tv_sec_lo;
    (void)tv_nsec;
}

static void on_capture_frame_ready(void * data, struct ext_image_copy_capture_frame_v1 * frame) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_debug(ctx, "mirror-extcopy::on_capture_frame_ready(): frame captured\n");

    if (backend->use_dmabuf) {
        const wlm_egl_format_t * format = wlm_egl_formats_find_drm(backend->frame_drm_format);
        if (format == NULL) {
            wlm_log_error("mirror-extcopy::on_capture_frame_ready(): failed to find GL format for drm format\n");
            backend_cancel(ctx, backend);
            return;
        }

        dmabuf_t * dmabuf = wlm_wayland_dmabuf_get_raw_buffer(ctx);
        if (dmabuf == NULL) {
            wlm_log_error("mirror-extcopy::on_capture_frame_ready(): DMA-BUF disappeared\n");
            backend_cancel(ctx, backend);
            return;
        }

        // TODO: invert_y?
        if (!wlm_egl_dmabuf_import(ctx, dmabuf, format, false, false)) {
            wlm_log_error("mirror-extcopy::on_capture_frame_ready(): failed to import dmabuf\n");
            backend_cancel(ctx, backend);
            return;
        }

        wlm_log_debug(ctx, "mirror-extcopy::on_capture_frame_ready(): frame captured and DMA-BUF imported\n");
    } else {
        const wlm_egl_format_t * format = wlm_egl_formats_find_shm(backend->frame_shm_format);
        if (format == NULL) {
            wlm_log_error("mirror-extcopy::on_capture_frame_ready(): failed to find GL format for shm format\n");
            backend_cancel(ctx, backend);
            return;
        }

        void * shm_addr = wlm_wayland_shm_get_addr(ctx);
        if (shm_addr == NULL) {
            wlm_log_error("mirror-screencopy::on_ready(): shm buffer addr disappeared\n");
            backend_cancel(ctx, backend);
            return;
        }

        // TODO: invert_y?
        if (!wlm_egl_shm_import(ctx, shm_addr, format, backend->frame_width, backend->frame_height, backend->frame_shm_stride, false, false)) {
            wlm_log_error("mirror-screencopy::on_ready(): shm buffer import failed\n");
            backend_cancel(ctx, backend);
            return;
        }
    }

    backend->state = STATE_READY;
    backend->header.fail_count = 0;

    (void)frame;
}

static void on_capture_frame_failed(void * data, struct ext_image_copy_capture_frame_v1 * frame, uint32_t reason) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_error("mirror-extcopy::on_capture_frame_failed(): failed to capture frame, reason = %d\n", reason);
    backend_cancel(ctx, backend);

    (void)frame;
}

static const struct ext_image_copy_capture_frame_v1_listener capture_frame_listener = {
    .transform = on_capture_frame_transform,
    .damage = on_capture_frame_damage,
    .presentation_time = on_capture_frame_presentation_time,
    .ready = on_capture_frame_ready,
    .failed = on_capture_frame_failed,
};

// --- backend event handlers ---

static void do_capture(ctx_t * ctx) {
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    if (backend->state == STATE_INIT || backend->state == STATE_CANCELED) {
        backend->capture_source = ext_output_image_capture_source_manager_v1_create_source(ctx->wl.output_capture_source_manager, ctx->mirror.current_target->output);

        wlm_log_debug(ctx, "mirror-extcopy::do_capture(): creating capture session\n");
        backend->state = STATE_WAIT_BUFFER_INFO;
        backend->capture_session = ext_image_copy_capture_manager_v1_create_session(ctx->wl.copy_capture_manager, backend->capture_source, ctx->opt.show_cursor ? EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS : 0);
        ext_image_copy_capture_session_v1_add_listener(backend->capture_session, &capture_session_listener, (void *)ctx);
    } else if (backend->state == STATE_READY) {
        if (backend->capture_frame != NULL) {
            ext_image_copy_capture_frame_v1_destroy(backend->capture_frame);
            backend->capture_frame = NULL;
        }

        wlm_log_debug(ctx, "mirror-extcopy::do_capture(): capturing frame\n");
        backend->capture_frame = ext_image_copy_capture_session_v1_create_frame(backend->capture_session);
        ext_image_copy_capture_frame_v1_add_listener(backend->capture_frame, &capture_frame_listener, (void *)ctx);

        struct wl_buffer * buffer = NULL;
        if (backend->use_dmabuf) {
            buffer = wlm_wayland_dmabuf_get_buffer(ctx);
        } else {
            buffer = wlm_wayland_shm_get_buffer(ctx);
        }

        if (buffer == NULL) {
            wlm_log_error("mirror-extcopy::do_capture(): buffer disappeared\n");
            backend_cancel(ctx, backend);
            return;
        }

        ext_image_copy_capture_frame_v1_attach_buffer(backend->capture_frame, buffer);
        ext_image_copy_capture_frame_v1_capture(backend->capture_frame);
        backend->state = STATE_WAIT_READY;
    } else if (backend->state == STATE_WAIT_READY) {
        // nop
    }
}

static void do_cleanup(ctx_t * ctx) {
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_debug(ctx, "mirror-extcopy::do_cleanup(): destroying mirror-extcopy objects\n");
    extcopy_session_cleanup(ctx, backend);

    free(backend);
    ctx->mirror.backend = NULL;
}

static void on_options_updated(ctx_t * ctx) {
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_debug(ctx, "mirror-extcopy::on_options_updated(): options updated, restarting capture\n");
    extcopy_session_cleanup(ctx, backend);
    backend->state = STATE_INIT;
    do_capture(ctx);
}

// --- wlm_mirror_extcopy_init ---

void wlm_mirror_extcopy_shm_init(ctx_t * ctx) {
    // check for required protocols
    if (ctx->wl.shm == NULL) {
        wlm_log_error("mirror-extcopy::shm_init(): missing wl_shm protocol\n");
        return;
    }

    if (ctx->wl.copy_capture_manager == NULL) {
        wlm_log_error("mirror-extcopy::shm_init(): missing ext_image_copy_capture protocol\n");
        return;
    }

    if (ctx->wl.output_capture_source_manager == NULL) {
        wlm_log_error("mirror-extcopy::shm_init(): missing ext_output_image_capture_source_manager protocol\n");
        return;
    }

    // allocate backend context structure
    extcopy_mirror_backend_t * backend = calloc(1, sizeof (extcopy_mirror_backend_t));
    if (backend == NULL) {
        wlm_log_error("mirror-extcopy::shm_init(): failed to allocate backend state\n");
        return;
    }

    backend->header.do_capture = do_capture;
    backend->header.do_cleanup = do_cleanup;
    backend->header.on_options_updated = on_options_updated;
    backend->header.fail_count = 0;
    backend->use_dmabuf = false;

    backend->capture_source = NULL;
    backend->capture_session = NULL;
    backend->capture_frame = NULL;

    backend->frame_width = 0;
    backend->frame_height = 0;
    backend->frame_shm_stride = 0;
    backend->frame_shm_format = 0;
    backend->frame_drm_format = 0;
    backend->frame_drm_modifiers = NULL;
    backend->frame_num_drm_modifiers = 0;

    backend->state = STATE_INIT;

    // set backend object as current backend
    ctx->mirror.backend = (mirror_backend_t *)backend;

    // create shm pool
    if (!wlm_wayland_shm_create_pool(ctx)) {
        wlm_log_error("mirror-extcopy::shm_init(): failed to create shm pool\n");
        wlm_mirror_backend_fail(ctx);
        return;
    }
}

void wlm_mirror_extcopy_dmabuf_init(ctx_t * ctx) {
    // check for required protocols
    if (ctx->wl.copy_capture_manager == NULL) {
        wlm_log_error("mirror-extcopy::dmabuf_init(): missing ext_image_copy_capture protocol\n");
        return;
    }

    if (ctx->wl.output_capture_source_manager == NULL) {
        wlm_log_error("mirror-extcopy::dmabuf_init(): missing ext_output_image_capture_source_manager protocol\n");
        return;
    }

    // allocate backend context structure
    extcopy_mirror_backend_t * backend = calloc(1, sizeof (extcopy_mirror_backend_t));
    if (backend == NULL) {
        wlm_log_error("mirror-extcopy::dmabuf_init(): failed to allocate backend state\n");
        return;
    }

    backend->header.do_capture = do_capture;
    backend->header.do_cleanup = do_cleanup;
    backend->header.on_options_updated = on_options_updated;
    backend->header.fail_count = 0;
    backend->use_dmabuf = true;

    backend->capture_source = NULL;
    backend->capture_session = NULL;
    backend->capture_frame = NULL;

    backend->frame_width = 0;
    backend->frame_height = 0;
    backend->frame_shm_stride = 0;
    backend->frame_shm_format = 0;
    backend->frame_drm_format = 0;
    backend->frame_drm_modifiers = NULL;
    backend->frame_num_drm_modifiers = 0;

    backend->state = STATE_INIT;

    // set backend object as current backend
    ctx->mirror.backend = (mirror_backend_t *)backend;
}
