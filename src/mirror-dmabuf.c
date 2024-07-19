#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "context.h"
#include "mirror-dmabuf.h"
#include <EGL/eglext.h>
#include "linux-dmabuf-unstable-v1.h"

static void dmabuf_frame_cleanup(dmabuf_mirror_backend_t * backend) {
    // destroy dmabuf frame object
    if (backend->dmabuf_frame != NULL) {
        zwlr_export_dmabuf_frame_v1_destroy(backend->dmabuf_frame);
        backend->dmabuf_frame = NULL;
    }

    // close dmabuf file descriptors
    for (unsigned int i = 0; i < backend->dmabuf.planes; i++) {
        if (backend->dmabuf.fds[i] != -1) close(backend->dmabuf.fds[i]);
    }

    free(backend->dmabuf.fds);
    free(backend->dmabuf.offsets);
    free(backend->dmabuf.strides);

    backend->dmabuf.width = 0;
    backend->dmabuf.height = 0;
    backend->dmabuf.drm_format = 0;
    backend->dmabuf.planes = 0;
    backend->dmabuf.fds = NULL;
    backend->dmabuf.offsets = NULL;
    backend->dmabuf.strides = NULL;
    backend->dmabuf.modifier = 0;
}

static void backend_cancel(dmabuf_mirror_backend_t * backend) {
    log_error("mirror-dmabuf::backend_cancel(): cancelling capture due to error\n");

    dmabuf_frame_cleanup(backend);
    backend->state = STATE_CANCELED;
    backend->header.fail_count++;
}

// --- dmabuf_frame event handlers ---

static void on_frame(
    void * data, struct zwlr_export_dmabuf_frame_v1 * frame,
    uint32_t width, uint32_t height, uint32_t x, uint32_t y,
    uint32_t buffer_flags, uint32_t frame_flags, uint32_t format,
    uint32_t mod_high, uint32_t mod_low, uint32_t num_objects
) {
    ctx_t * ctx = (ctx_t *)data;
    dmabuf_mirror_backend_t * backend = (dmabuf_mirror_backend_t *)ctx->mirror.backend;

    log_debug(ctx, "mirror-dmabuf::on_frame(): received %dx%d frame with %d objects\n", width, height, num_objects);
    if (backend->state != STATE_WAIT_FRAME) {
        log_error("mirror-dmabuf::on_frame(): got frame while in state %d\n", backend->state);
        backend_cancel(backend);
        return;
    } else if (num_objects > MAX_PLANES) {
        log_error("mirror-dmabuf::on_frame(): got frame with more than %d objects\n", MAX_PLANES);
        backend_cancel(backend);
        return;
    }

    uint32_t unhandled_buffer_flags = buffer_flags & ~(
        ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT
    );
    if (unhandled_buffer_flags != 0) {
        log_warn("mirror-dmabuf::on_frame(): frame uses unhandled buffer flags, buffer_flags = {");
        if (buffer_flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT) fprintf(stderr, "Y_INVERT, ");
        if (buffer_flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_INTERLACED) fprintf(stderr, "INTERLACED, ");
        if (buffer_flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_BOTTOM_FIRST) fprintf(stderr, "BOTTOM_FIRST, ");
        fprintf(stderr, "}\n");
    }

    uint32_t unhandled_frame_flags = frame_flags & ~(
        ZWLR_EXPORT_DMABUF_FRAME_V1_FLAGS_TRANSIENT
    );
    if (unhandled_frame_flags != 0) {
        log_warn("mirror-dmabuf::on_frame(): frame uses unhandled frame flags, frame_flags = {");
        if (frame_flags & ZWLR_EXPORT_DMABUF_FRAME_V1_FLAGS_TRANSIENT) fprintf(stderr, "TRANSIENT, ");
        fprintf(stderr, "}\n");
    }

    backend->dmabuf.planes = 0;
    backend->dmabuf.fds = malloc(num_objects * sizeof (int));
    backend->dmabuf.offsets = malloc(num_objects * sizeof (uint32_t));
    backend->dmabuf.strides = malloc(num_objects * sizeof (uint32_t));
    backend->dmabuf.modifier = ((uint64_t)mod_high << 32) | mod_low;
    if (backend->dmabuf.fds == NULL || backend->dmabuf.offsets == NULL) {
        log_error("mirror-dmabuf::on_frame(): failed to allocate dmabuf storage\n");
        backend_cancel(backend);
        return;
    }

    // save dmabuf frame info
    backend->x = x;
    backend->y = y;
    backend->buffer_flags = buffer_flags;
    backend->frame_flags = frame_flags;
    backend->dmabuf.width = width;
    backend->dmabuf.height = height;
    backend->dmabuf.drm_format = format;
    backend->dmabuf.planes = num_objects;

    log_debug(ctx, "mirror-dmabuf::on_frame(): w=%d h=%d gl_format=%x drm_format=%08x drm_modifier=%016lx\n",
        backend->dmabuf.width, backend->dmabuf.height, GL_RGB8_OES, backend->dmabuf.drm_format, backend->dmabuf.modifier
    );

    for (size_t i = 0; i < num_objects; i++) {
        backend->dmabuf.fds[i] = -1;
    }

    // update dmabuf frame state machine
    backend->state = STATE_WAIT_OBJECTS;
    backend->processed_objects = 0;

    (void)frame;
}

static void on_object(
    void * data, struct zwlr_export_dmabuf_frame_v1 * frame,
    uint32_t index, int32_t fd, uint32_t size,
    uint32_t offset, uint32_t stride, uint32_t plane_index
) {
    ctx_t * ctx = (ctx_t *)data;
    dmabuf_mirror_backend_t * backend = (dmabuf_mirror_backend_t *)ctx->mirror.backend;

    log_debug(ctx, "mirror-dmabuf::on_object(): fd=%d offset=% 10d stride=% 10d\n",
        fd, offset, stride
    );
    if (backend->state != STATE_WAIT_OBJECTS) {
        log_error("mirror-dmabuf::on_object(): got object while in state %d\n", backend->state);
        close(fd);
        backend_cancel(backend);
        return;
    } else if (index >= backend->dmabuf.planes) {
        log_error("mirror-dmabuf::on_object(): got object with out-of-bounds index %d\n", index);
        close(fd);
        backend_cancel(backend);
        return;
    }


    backend->dmabuf.fds[index] = fd;
    backend->dmabuf.offsets[index] = offset;
    backend->dmabuf.strides[index] = stride;

    backend->processed_objects++;
    if (backend->processed_objects == backend->dmabuf.planes) {
        backend->state = STATE_WAIT_READY;
    }

    (void)frame;
    (void)size;
    (void)plane_index;
}

static void on_ready(
    void * data, struct zwlr_export_dmabuf_frame_v1 * frame,
    uint32_t sec_hi, uint32_t sec_lo, uint32_t nsec
) {
    ctx_t * ctx = (ctx_t *)data;
    dmabuf_mirror_backend_t * backend = (dmabuf_mirror_backend_t *)ctx->mirror.backend;

    log_debug(ctx, "mirror-dmabuf::on_ready(): frame is ready\n");
    if (backend->state != STATE_WAIT_READY) {
        log_error("dmabuf_frame: got ready while in state %d\n", backend->state);
        backend_cancel(backend);
        return;
    }

    if (!dmabuf_to_texture(ctx, &backend->dmabuf)) {
        log_error("mirror-dmabuf::on_ready(): failed to import dmabuf\n");
        backend_cancel(backend);
    }

    ctx->egl.format = GL_RGB8_OES; // FIXME: find out actual format
    ctx->egl.texture_region_aware = false;
    ctx->egl.texture_initialized = true;

    // set buffer flags only if changed
    bool invert_y = backend->buffer_flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT;
    if (ctx->mirror.invert_y != invert_y) {
        ctx->mirror.invert_y = invert_y;
        update_uniforms(ctx);
    }

    // set texture size and aspect ratio only if changed
    if (backend->dmabuf.width != ctx->egl.width || backend->dmabuf.height != ctx->egl.height) {
        ctx->egl.width = backend->dmabuf.width;
        ctx->egl.height = backend->dmabuf.height;
        resize_viewport(ctx);
    }

    dmabuf_frame_cleanup(backend);
    backend->state = STATE_READY;
    backend->header.fail_count = 0;

    (void)frame;
    (void)sec_hi;
    (void)sec_lo;
    (void)nsec;
}

static void on_cancel(
    void * data, struct zwlr_export_dmabuf_frame_v1 * frame,
    enum zwlr_export_dmabuf_frame_v1_cancel_reason reason
) {
    ctx_t * ctx = (ctx_t *)data;
    dmabuf_mirror_backend_t * backend = (dmabuf_mirror_backend_t *)ctx->mirror.backend;

    log_debug(ctx, "mirror-dmabuf::on_cancel(): frame was canceled\n");

    dmabuf_frame_cleanup(backend);
    backend->state = STATE_CANCELED;

    switch (reason) {
        case ZWLR_EXPORT_DMABUF_FRAME_V1_CANCEL_REASON_PERMANENT:
            log_error("mirror-dmabuf::on_cancel(): permanent cancellation\n");
            backend->header.fail_count++;
            break;

        case ZWLR_EXPORT_DMABUF_FRAME_V1_CANCEL_REASON_TEMPORARY:
            log_error("mirror-dmabuf::on_cancel(): temporary cancellation\n");
            backend->header.fail_count++;
            break;

        case ZWLR_EXPORT_DMABUF_FRAME_V1_CANCEL_REASON_RESIZING:
            log_debug(ctx, "mirror-dmabuf::on_cancel(): cancellation due to output resize\n");
            break;

        default:
            log_error("mirror-dmabuf::on_cancel(): unknown cancellation reason %d\n", reason);
            backend->header.fail_count++;
            break;
    }

    (void)frame;
}

static const struct zwlr_export_dmabuf_frame_v1_listener dmabuf_frame_listener = {
    .frame = on_frame,
    .object = on_object,
    .ready = on_ready,
    .cancel = on_cancel
};

// --- backend event handlers ---

static void do_capture(ctx_t * ctx) {
    dmabuf_mirror_backend_t * backend = (dmabuf_mirror_backend_t *)ctx->mirror.backend;

    if (backend->state == STATE_READY || backend->state == STATE_CANCELED) {
        // clear frame state for next frame
        backend->x = 0;
        backend->y = 0;
        backend->buffer_flags = 0;
        backend->frame_flags = 0;
        dmabuf_frame_cleanup(backend);

        backend->state = STATE_WAIT_FRAME;
        backend->processed_objects = 0;

        // create wlr_dmabuf_export_frame
        backend->dmabuf_frame = zwlr_export_dmabuf_manager_v1_capture_output(
            ctx->wl.dmabuf_manager, ctx->opt.show_cursor, ctx->mirror.current_target->output
        );
        if (backend->dmabuf_frame == NULL) {
            log_error("mirror-dmabuf::do_capture(): failed to create wlr_dmabuf_export_frame\n");
            backend_fail(ctx);
            return;
        }

        // add wlr_dmabuf_export_frame event listener
        // - for frame event
        // - for object event
        // - for ready event
        // - for cancel event
        zwlr_export_dmabuf_frame_v1_add_listener(backend->dmabuf_frame, &dmabuf_frame_listener, (void *)ctx);
    }

}

static void do_cleanup(ctx_t * ctx) {
    dmabuf_mirror_backend_t * backend = (dmabuf_mirror_backend_t *)ctx->mirror.backend;

    log_debug(ctx, "mirror-dmabuf::do_cleanup(): destroying mirror-dmabuf objects\n");
    dmabuf_frame_cleanup(backend);

    free(backend);
    ctx->mirror.backend = NULL;
}

// --- init_mirror_dmabuf ---

void init_mirror_dmabuf(ctx_t * ctx) {
    // check for required protocols
    if (ctx->wl.dmabuf_manager == NULL) {
        log_error("mirror-dmabuf::init(): missing wlr_export_dmabuf_manager protocol\n");
        return;
    }

    // allocate backend context structure
    dmabuf_mirror_backend_t * backend = calloc(1, sizeof (dmabuf_mirror_backend_t));
    if (backend == NULL) {
        log_error("mirror-dmabuf::init(): failed to allocate backend state\n");
        return;
    }

    // initialize context structure
    backend->header.do_capture = do_capture;
    backend->header.do_cleanup = do_cleanup;
    backend->header.fail_count = 0;

    backend->dmabuf_frame = NULL;

    backend->x = 0;
    backend->y = 0;
    backend->buffer_flags = 0;
    backend->frame_flags = 0;
    backend->dmabuf.width = 0;
    backend->dmabuf.height = 0;
    backend->dmabuf.drm_format = 0;
    backend->dmabuf.planes = 0;
    backend->dmabuf.fds = NULL;
    backend->dmabuf.offsets = NULL;
    backend->dmabuf.strides = NULL;
    backend->dmabuf.modifier = 0;

    backend->state = STATE_READY;
    backend->processed_objects = 0;

    // set backend object as current backend
    ctx->mirror.backend = (mirror_backend_t *)backend;
}
