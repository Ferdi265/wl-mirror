#include <wlm/mirror-extcopy.h>
#include <wlm/context.h>
#include <wlm/wayland/shm.h>
#include <wlm/wayland/dmabuf.h>
#include <stdlib.h>
#include <string.h>

static void extcopy_frame_cleanup(extcopy_mirror_backend_t * backend) {
    if (backend->capture_frame != NULL) ext_image_copy_capture_frame_v1_destroy(backend->capture_frame);
    if (backend->modifiers != NULL) free(backend->modifiers);

    backend->capture_frame = NULL;
    backend->modifiers = NULL;
    backend->width = 0;
    backend->height = 0;
    backend->has_shm_format = false;
    backend->has_drm_format = false;
    backend->shm_format = 0;
    backend->drm_format = 0;
    backend->num_modifiers = 0;
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
    backend->width = width;
    backend->height = height;

    (void)session;
}

static void on_capture_session_shm_format(void * data, struct ext_image_copy_capture_session_v1 * session, uint32_t shm_format) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    if (!backend->has_shm_format) {
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_shm_format(): shm_format = %x\n", shm_format);
        backend->has_shm_format = true;
        backend->shm_format = shm_format;
    } else {
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_shm_format(): alternate shm_format = %x (ignoring)\n", shm_format);
        return;
    }

    (void)session;
}

static void on_capture_session_dmabuf_format(void * data, struct ext_image_copy_capture_session_v1 * session, uint32_t drm_format, struct wl_array * modifiers_arr) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    if (!backend->has_drm_format) {
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_dmabuf_format(): drm_format = %x\n", drm_format);
        backend->has_drm_format = true;
        backend->drm_format = drm_format;
    } else {
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_dmabuf_format(): alternate drm_format = %x (ignoring)\n", drm_format);
        return;
    }

    if (modifiers_arr->size % sizeof (uint64_t) != 0) {
        wlm_log_error("mirror-extcopy::on_capture_session_dmabuf_format(): modifiers have invalid size %zd, expected multiple of %zd\n", modifiers_arr->size, sizeof (uint64_t));
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_dmabuf_format(): falling back to shm\n");
        backend->has_drm_format = false;
        return;
    }

    if (backend->modifiers != NULL) {
        free(backend->modifiers);
        backend->modifiers = NULL;
    }

    backend->num_modifiers = modifiers_arr->size / sizeof (uint64_t);
    backend->modifiers = (uint64_t *)calloc(backend->num_modifiers, sizeof (uint64_t));
    if (backend->modifiers == NULL) {
        wlm_log_error("mirror-extcopy::on_capture_session_dmabuf_format(): failed to allocate modifiers array\n");
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_dmabuf_format(): falling back to shm\n");
        backend->has_drm_format = false;
        return;
    }

    memcpy(backend->modifiers, modifiers_arr->data, modifiers_arr->size);

    (void)session;
}

static void on_capture_session_dmabuf_device(void * data, struct ext_image_copy_capture_session_v1 * session, struct wl_array * device) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_dmabuf_device(): opening device\n");
    if (device->size != sizeof (dev_t)) {
        wlm_log_error("mirror-extcopy::on_capture_session_dmabuf_device(): array size mismatch: %zd != %zd\n", device->size, sizeof (dev_t));
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_dmabuf_device(): falling back to shm\n");
        backend->has_drm_format = false;
        return;
    }

    dev_t device_id;
    memcpy(&device_id, device->data, sizeof (dev_t));

    if (!wlm_wayland_dmabuf_open_device(ctx, device_id)) {
        wlm_log_error("mirror-extcopy::on_capture_session_dmabuf_device(): failed to open device\n");
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_dmabuf_device(): falling back to shm\n");
        backend->has_drm_format = false;
        return;
    }

    (void)session;
}

static void on_shmbuf_alloc(ctx_t * ctx);
static void on_dmabuf_allocated(ctx_t * ctx, bool success);
static void on_capture_session_done(void * data, struct ext_image_copy_capture_session_v1 * session) {
    ctx_t * ctx = (ctx_t *)data;
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    if (backend->modifiers == NULL) {
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_done(): allocating SHM buffer\n");
        on_shmbuf_alloc(ctx);
    } else {
        wlm_log_debug(ctx, "mirror-extcopy::on_capture_session_done(): allocating DMA-BUF\n");
        wlm_wayland_dmabuf_alloc(ctx, backend->drm_format, backend->width, backend->height, backend->modifiers, backend->num_modifiers, on_dmabuf_allocated);
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

static void on_shmbuf_alloc(ctx_t * ctx) {
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_warn("mirror-extcopy::on_shmbuf_alloc(): FIXME: use correct stride calculation\n");
    uint32_t stride = backend->width * 4 /* TODO: fix this!!!! */;
    bool success = wlm_wayland_shm_alloc(ctx, backend->shm_format, backend->width, backend->height, stride);

    if (!success) {
        wlm_log_error("mirror-extcopy::on_shmbuf_alloc(): failed to allocate SHM buffer\n");
        backend_cancel(ctx, backend);
        return;
    }

    wlm_log_debug(ctx, "mirror-extcopy::on_shmbuf_alloc(): SHM buffer allocated\n");
    backend->state = STATE_READY;
}

static void on_dmabuf_allocated(ctx_t * ctx, bool success) {
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    if (!success) {
        wlm_log_error("mirror-extcopy::on_dmabuf_allocated(): failed to allocate DMA-BUF\n");
        wlm_log_debug(ctx, "mirror-extcopy::on_dmabuf_allocated(): falling back to shm\n");
        backend->has_drm_format = false;
        on_shmbuf_alloc(ctx);
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

    if (backend->has_drm_format) {
        if (!wlm_egl_dmabuf_to_texture(ctx, wlm_wayland_dmabuf_get_raw_buffer(ctx))) {
            wlm_log_error("mirror-extcopy::on_capture_frame_ready(): failed to import dmabuf\n");
            backend_cancel(ctx, backend);
            return;
        }

        ctx->egl.format = GL_RGB8_OES; // FIXME: find out actual format
        ctx->egl.texture_region_aware = false;
        ctx->egl.texture_initialized = true;

        // set texture size and aspect ratio only if changed
        if (backend->width != ctx->egl.width || backend->height != ctx->egl.height) {
            ctx->egl.width = backend->width;
            ctx->egl.height = backend->height;
            wlm_egl_resize_viewport(ctx);
        }

        wlm_log_debug(ctx, "mirror-extcopy::on_capture_frame_ready(): frame captured and DMA-BUF imported\n");
    } else {
        wlm_log_error("mirror-extcopy::on_capture_frame_ready(): SHM buffer import not implemented\n");
        backend_cancel(ctx, backend);
        return;
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

        ext_image_copy_capture_frame_v1_attach_buffer(backend->capture_frame, wlm_wayland_dmabuf_get_buffer(ctx));
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

void wlm_mirror_extcopy_init(ctx_t * ctx) {
    // check for required protocols
    if (ctx->wl.copy_capture_manager == NULL) {
        wlm_log_error("mirror-extcopy::init(): missing ext_image_copy_capture protocol\n");
        return;
    }

    if (ctx->wl.output_capture_source_manager == NULL) {
        wlm_log_error("mirror-extcopy::init(): missing ext_output_image_capture_source_manager protocol\n");
        return;
    }

    // allocate backend context structure
    extcopy_mirror_backend_t * backend = calloc(1, sizeof (extcopy_mirror_backend_t));
    if (backend == NULL) {
        wlm_log_error("mirror-extcopy::init(): failed to allocate backend state\n");
        return;
    }

    backend->header.do_capture = do_capture;
    backend->header.do_cleanup = do_cleanup;
    backend->header.on_options_updated = on_options_updated;
    backend->header.fail_count = 0;

    backend->capture_source = NULL;
    backend->capture_session = NULL;
    backend->capture_frame = NULL;

    backend->width = 0;
    backend->height = 0;
    backend->shm_format = 0;
    backend->drm_format = 0;
    backend->modifiers = NULL;
    backend->num_modifiers = 0;

    backend->state = STATE_INIT;

    // set backend object as current backend
    ctx->mirror.backend = (mirror_backend_t *)backend;
}
