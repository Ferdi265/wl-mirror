#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "context.h"
#include "mirror-dmabuf.h"
#include <EGL/eglext.h>
#include "linux-dmabuf-unstable-v1.h"

static void dmabuf_frame_cleanup(dmabuf_mirror_backend_t * backend) {
    // destroy dmabuf frame object
    zwlr_export_dmabuf_frame_v1_destroy(backend->dmabuf_frame);
    backend->dmabuf_frame = NULL;

    // close dmabuf file descriptors
    for (unsigned int i = 0; i < backend->num_objects; i++) {
        if (backend->objects[i].fd != -1) close(backend->objects[i].fd);
        backend->objects[i].fd = -1;
    }
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
    } else if (num_objects > 4) {
        log_error("mirror-dmabuf::on_frame(): got frame with more than 4 objects\n");
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

    // save dmabuf frame info
    backend->width = width;
    backend->height = height;
    backend->x = x;
    backend->y = y;
    backend->buffer_flags = buffer_flags;
    backend->frame_flags = frame_flags;
    backend->format = format;
    backend->modifier_high = mod_high;
    backend->modifier_low = mod_low;
    backend->num_objects = num_objects;

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

    log_debug(ctx, "mirror-dmabuf::on_object(): received %d byte object with plane_index %d\n", size, plane_index);
    if (backend->state != STATE_WAIT_OBJECTS) {
        log_error("mirror-dmabuf::on_object(): got object while in state %d\n", backend->state);
        close(fd);
        backend_cancel(backend);
        return;
    } else if (index >= backend->num_objects) {
        log_error("mirror-dmabuf::on_object(): got object with out-of-bounds index %d\n", index);
        close(fd);
        backend_cancel(backend);
        return;
    }

    backend->objects[index].fd = fd;
    backend->objects[index].size = size;
    backend->objects[index].offset = offset;
    backend->objects[index].stride = stride;
    backend->objects[index].plane_index = plane_index;

    backend->processed_objects++;
    if (backend->processed_objects == backend->num_objects) {
        backend->state = STATE_WAIT_READY;
    }

    (void)frame;
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

    // create attribute array for EGLCreteImage from dmabuf
    int i = 0;
    EGLAttrib image_attribs[6 + 10 * 4 + 1];

    image_attribs[i++] = EGL_WIDTH;
    image_attribs[i++] = backend->width;
    image_attribs[i++] = EGL_HEIGHT;
    image_attribs[i++] = backend->height;
    image_attribs[i++] = EGL_LINUX_DRM_FOURCC_EXT;
    image_attribs[i++] = backend->format;

    if (backend->num_objects >= 1) {
        image_attribs[i++] = EGL_DMA_BUF_PLANE0_FD_EXT;
        image_attribs[i++] = backend->objects[0].fd;
        image_attribs[i++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
        image_attribs[i++] = backend->objects[0].offset;
        image_attribs[i++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
        image_attribs[i++] = backend->objects[0].stride;
        image_attribs[i++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
        image_attribs[i++] = backend->modifier_low;
        image_attribs[i++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
        image_attribs[i++] = backend->modifier_high;
    }

    if (backend->num_objects >= 2) {
        image_attribs[i++] = EGL_DMA_BUF_PLANE1_FD_EXT;
        image_attribs[i++] = backend->objects[1].fd;
        image_attribs[i++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
        image_attribs[i++] = backend->objects[1].offset;
        image_attribs[i++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
        image_attribs[i++] = backend->objects[1].stride;
        image_attribs[i++] = EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT;
        image_attribs[i++] = backend->modifier_low;
        image_attribs[i++] = EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT;
        image_attribs[i++] = backend->modifier_high;
    }

    if (backend->num_objects >= 3) {
        image_attribs[i++] = EGL_DMA_BUF_PLANE2_FD_EXT;
        image_attribs[i++] = backend->objects[2].fd;
        image_attribs[i++] = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
        image_attribs[i++] = backend->objects[2].offset;
        image_attribs[i++] = EGL_DMA_BUF_PLANE2_PITCH_EXT;
        image_attribs[i++] = backend->objects[2].stride;
        image_attribs[i++] = EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT;
        image_attribs[i++] = backend->modifier_low;
        image_attribs[i++] = EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT;
        image_attribs[i++] = backend->modifier_high;
    }

    if (backend->num_objects >= 4) {
        image_attribs[i++] = EGL_DMA_BUF_PLANE3_FD_EXT;
        image_attribs[i++] = backend->objects[3].fd;
        image_attribs[i++] = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
        image_attribs[i++] = backend->objects[3].offset;
        image_attribs[i++] = EGL_DMA_BUF_PLANE3_PITCH_EXT;
        image_attribs[i++] = backend->objects[3].stride;
        image_attribs[i++] = EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT;
        image_attribs[i++] = backend->modifier_low;
        image_attribs[i++] = EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT;
        image_attribs[i++] = backend->modifier_high;
    }

    image_attribs[i++] = EGL_NONE;

    // create EGLImage from dmabuf with attribute array
    EGLImage frame_image = eglCreateImage(ctx->egl.display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, image_attribs);
    if (frame_image == EGL_NO_IMAGE) {
        log_error("mirror-dmabuf::on_ready(): failed to create EGL image from dmabuf\n");
        backend_fail(ctx);
        return;
    }

    // convert EGLImage to GL texture
    glBindTexture(GL_TEXTURE_2D, ctx->egl.texture);
    ctx->egl.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, frame_image);
    ctx->egl.texture_region_aware = false;
    ctx->egl.texture_initialized = true;

    // destroy temporary EGLImage
    eglDestroyImage(ctx->egl.display, frame_image);

    // set buffer flags only if changed
    bool invert_y = backend->buffer_flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT;
    if (ctx->mirror.invert_y != invert_y) {
        ctx->mirror.invert_y = invert_y;
        update_uniforms(ctx);
    }

    // set texture size and aspect ratio only if changed
    if (backend->width != ctx->egl.width || backend->height != ctx->egl.height) {
        ctx->egl.width = backend->width;
        ctx->egl.height = backend->height;
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
        backend->width = 0;
        backend->height = 0;
        backend->x = 0;
        backend->y = 0;
        backend->buffer_flags = 0;
        backend->frame_flags = 0;
        backend->modifier_high = 0;
        backend->modifier_low = 0;
        backend->format = 0;
        backend->num_objects = 0;

        dmabuf_object_t empty_obj = {
            .fd = -1,
            .size = 0,
            .offset = 0,
            .stride = 0,
            .plane_index = 0
        };
        backend->objects[0] = empty_obj;
        backend->objects[1] = empty_obj;
        backend->objects[2] = empty_obj;
        backend->objects[3] = empty_obj;

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

    if (backend->dmabuf_frame != NULL) zwlr_export_dmabuf_frame_v1_destroy(backend->dmabuf_frame);

    for (int i = 0; i < 4; i++) {
        if (backend->objects[i].fd != -1) close(backend->objects[i].fd);
        backend->objects[i].fd = -1;
    }

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
    dmabuf_mirror_backend_t * backend = malloc(sizeof (dmabuf_mirror_backend_t));
    if (backend == NULL) {
        log_error("mirror-dmabuf::init(): failed to allocate backend state\n");
        return;
    }

    // initialize context structure
    backend->header.do_capture = do_capture;
    backend->header.do_cleanup = do_cleanup;
    backend->header.fail_count = 0;

    backend->dmabuf_frame = NULL;

    backend->width = 0;
    backend->height = 0;
    backend->x = 0;
    backend->y = 0;
    backend->buffer_flags = 0;
    backend->frame_flags = 0;
    backend->modifier_high = 0;
    backend->modifier_low = 0;
    backend->format = 0;
    backend->num_objects = 0;

    dmabuf_object_t empty_obj = {
        .fd = -1,
        .size = 0,
        .offset = 0,
        .stride = 0,
        .plane_index = 0
    };
    backend->objects[0] = empty_obj;
    backend->objects[1] = empty_obj;
    backend->objects[2] = empty_obj;
    backend->objects[3] = empty_obj;

    backend->state = STATE_READY;
    backend->processed_objects = 0;

    // set backend object as current backend
    ctx->mirror.backend = (mirror_backend_t *)backend;
}
