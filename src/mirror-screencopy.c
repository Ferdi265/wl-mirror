#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wlm/context.h>
#include <wlm/mirror-screencopy.h>
#include <wlm/egl/formats.h>
#include <sys/mman.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

static void backend_cancel(screencopy_mirror_backend_t * backend) {
    wlm_log_error("mirror-screencopy::backend_cancel(): cancelling capture due to error\n");

    // destroy screencopy frame object
    zwlr_screencopy_frame_v1_destroy(backend->screencopy_frame);
    backend->screencopy_frame = NULL;
    backend->state = STATE_CANCELED;
    backend->header.fail_count++;
}

// --- screencopy_frame event handlers ---

static void on_buffer(
    void * data, struct zwlr_screencopy_frame_v1 * frame,
    uint32_t format, uint32_t width, uint32_t height, uint32_t stride
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_debug(ctx, "mirror-screencopy::on_buffer(): received buffer offer for %dx%d+%d frame\n", width, height, stride);
    if (backend->state != STATE_WAIT_BUFFER) {
        wlm_log_error("mirror-screencopy::on_buffer(): got buffer event while in state %d\n", backend->state);
        backend_cancel(backend);
        return;
    }

    size_t new_size = stride * height;
    if (new_size > backend->shm_size) {
        if (backend->shm_buffer != NULL) {
            wl_buffer_destroy(backend->shm_buffer);
            backend->shm_buffer = NULL;
        }

        if (ftruncate(backend->shm_fd, new_size) == -1) {
            wlm_log_error("mirror-screencopy::on_buffer(): failed to grow shm buffer\n");
            backend_cancel(backend);
            return;
        }

#if __linux__
        void * new_addr = mremap(backend->shm_addr, backend->shm_size, new_size, MREMAP_MAYMOVE);
        if (new_addr == MAP_FAILED) {
            wlm_log_error("mirror-screencopy::on_buffer(): failed to remap shm buffer\n");
            backend_cancel(backend);
            return;
        }
#else
        void * new_addr = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, backend->shm_fd, 0);
        if (new_addr == MAP_FAILED) {
            wlm_log_error("mirror-screencopy::on_buffer(): failed to map new shm buffer\n");
            backend_cancel(backend);
            return;
        } else {
            munmap(backend->shm_addr, backend->shm_size);
        }
#endif

        backend->shm_addr = new_addr;
        backend->shm_size = new_size;

        wl_shm_pool_resize(backend->shm_pool, new_size);
    }

    bool new_buffer_needed =
        backend->frame_width != width ||
        backend->frame_height != height ||
        backend->frame_stride != stride ||
        backend->frame_format != format;

    if (backend->shm_buffer != NULL && new_buffer_needed) {
        wl_buffer_destroy(backend->shm_buffer);
        backend->shm_buffer = NULL;
    }

    if (backend->shm_buffer == NULL) {
        backend->shm_buffer = wl_shm_pool_create_buffer(
            backend->shm_pool, 0,
            width, height,
            stride, format
        );
        if (backend->shm_buffer == NULL) {
            wlm_log_error("mirror-screencopy::on_buffer(): failed to create wl_buffer\n");
            backend_cancel(backend);
            return;
        }
    }

    backend->frame_width = width;
    backend->frame_height = height;
    backend->frame_stride = stride;
    backend->frame_format = format;
    backend->state = STATE_WAIT_BUFFER_DONE;

    (void)frame;
}

static void on_linux_dmabuf(
    void * data, struct zwlr_screencopy_frame_v1 * frame,
    uint32_t format, uint32_t width, uint32_t height
) {
    (void)data;
    (void)frame;
    (void)format;
    (void)width;
    (void)height;
}

static void on_buffer_done(
    void * data, struct zwlr_screencopy_frame_v1 * frame
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_debug(ctx, "mirror-screencopy::on_buffer_done(): received buffer done event\n");
    if (backend->state != STATE_WAIT_BUFFER_DONE) {
        wlm_log_error("mirror-screencopy::on_buffer_done(): received buffer_done without supported buffer offer\n");
        backend_cancel(backend);
        return;
    }

    backend->state = STATE_WAIT_FLAGS;
    zwlr_screencopy_frame_v1_copy(backend->screencopy_frame, backend->shm_buffer);

    (void)frame;
}

static void on_damage(
    void * data, struct zwlr_screencopy_frame_v1 * frame,
    uint32_t x, uint32_t y, uint32_t width, uint32_t height
) {
    (void)data;
    (void)frame;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

static void on_flags(
    void * data, struct zwlr_screencopy_frame_v1 * frame,
    uint32_t flags
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_debug(ctx, "mirror-screencopy::on_flags(): received flags event\n");
    if (backend->state != STATE_WAIT_FLAGS) {
        wlm_log_error("mirror-screencopy::on_flags(): received unexpected flags event\n");
        backend_cancel(backend);
        return;
    }

    backend->frame_flags = flags;
    backend->state = STATE_WAIT_READY;

    (void)frame;
}

static void on_ready(
    void * data, struct zwlr_screencopy_frame_v1 * frame,
    uint32_t sec_hi, uint32_t sec_lo, uint32_t nsec
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    if (ctx->opt.verbose) {
        wlm_log_debug(ctx, "mirror-screencopy::on_ready(): received ready event with width: %d, height: %d, stride: %d, format: %c%c%c%c\n",
            backend->frame_width, backend->frame_height,
            backend->frame_stride,
            (backend->frame_format >> 24) & 0xff,
            (backend->frame_format >> 16) & 0xff,
            (backend->frame_format >> 8) & 0xff,
            (backend->frame_format >> 0) & 0xff
        );
    }

    // find correct texture format
    const wlm_egl_format_t * format = wlm_egl_formats_find_shm(backend->frame_format);
    if (format == NULL) {
        wlm_log_error("mirror-screencopy::on_ready(): failed to find GL format for shm format\n");
        wlm_mirror_backend_fail(ctx);
        return;
    }

    // store frame data into texture
    glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, backend->frame_stride / (format->bpp / 8));
    glTexImage2D(GL_TEXTURE_2D,
        0, format->gl_format, backend->frame_width, backend->frame_height,
        0, format->gl_format, format->gl_type, backend->shm_addr
    );
    glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);
    ctx->egl.format = format->gl_format;
    ctx->egl.texture_region_aware = true;
    ctx->egl.texture_initialized = true;

    // set buffer flags
    bool invert_y = backend->frame_flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT;
    if (ctx->mirror.invert_y != invert_y) {
        ctx->mirror.invert_y = invert_y;
        wlm_egl_update_uniforms(ctx);
    }

    // set texture size and aspect ratio only if changed
    if (backend->frame_width != ctx->egl.width || backend->frame_height != ctx->egl.height) {
        ctx->egl.width = backend->frame_width;
        ctx->egl.height = backend->frame_height;
        wlm_egl_resize_viewport(ctx);
    }

    zwlr_screencopy_frame_v1_destroy(backend->screencopy_frame);
    backend->screencopy_frame = NULL;
    backend->state = STATE_READY;
    backend->header.fail_count = 0;

    (void)frame;
    (void)sec_hi;
    (void)sec_lo;
    (void)nsec;
}

static void on_failed(
    void * data, struct zwlr_screencopy_frame_v1 * frame
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_debug(ctx, "mirror-screencopy::on_failed(): received cancel event\n");

    backend_cancel(backend);

    (void)frame;
}

static const struct zwlr_screencopy_frame_v1_listener screencopy_frame_listener = {
    .buffer = on_buffer,
    .linux_dmabuf = on_linux_dmabuf,
    .buffer_done = on_buffer_done,
    .damage = on_damage,
    .flags = on_flags,
    .ready = on_ready,
    .failed = on_failed
};

// --- backend event handlers ---

static void do_capture(ctx_t * ctx) {
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    if (backend->state == STATE_READY || backend->state == STATE_CANCELED) {
        // clear frame state for next frame
        backend->frame_flags = 0;
        backend->state = STATE_WAIT_BUFFER;

        // create screencopy_frame
        if (ctx->opt.has_region) {
            backend->screencopy_frame = zwlr_screencopy_manager_v1_capture_output_region(
                ctx->wl.screencopy_manager, ctx->opt.show_cursor, ctx->mirror.current_target->output,
                ctx->mirror.current_target->x + ctx->mirror.current_region.x,
                ctx->mirror.current_target->y + ctx->mirror.current_region.y,
                ctx->mirror.current_region.width,
                ctx->mirror.current_region.height
            );
        } else {
            backend->screencopy_frame = zwlr_screencopy_manager_v1_capture_output(
                ctx->wl.screencopy_manager, ctx->opt.show_cursor, ctx->mirror.current_target->output
            );
        }
        if (backend->screencopy_frame == NULL) {
            wlm_log_error("do_capture: failed to create wlr_screencopy_frame\n");
            wlm_mirror_backend_fail(ctx);
        }

        // add screencopy_frame event listener
        // - for buffer event
        // - for buffer_done event
        // - for flags event
        // - for ready event
        // - for failed event
        zwlr_screencopy_frame_v1_add_listener(backend->screencopy_frame, &screencopy_frame_listener, (void *)ctx);
    }
}

static void do_cleanup(ctx_t * ctx) {
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_debug(ctx, "mirror-screencopy::do_cleanup(): destroying mirror-screencopy objects\n");

    if (backend->screencopy_frame != NULL) zwlr_screencopy_frame_v1_destroy(backend->screencopy_frame);
    if (backend->shm_buffer != NULL) wl_buffer_destroy(backend->shm_buffer);
    if (backend->shm_pool != NULL) wl_shm_pool_destroy(backend->shm_pool);
    if (backend->shm_addr != NULL) munmap(backend->shm_addr, backend->shm_size);
    if (backend->shm_fd != -1) close(backend->shm_fd);

    free(backend);
    ctx->mirror.backend = NULL;
}

// --- init_mirror_screencopy ---

void wlm_mirror_screencopy_init(ctx_t * ctx) {
    // check for required protocols
    if (ctx->wl.shm == NULL) {
        wlm_log_error("mirror-screencopy::init(): missing wl_shm protocol\n");
        return;
    } else if (ctx->wl.screencopy_manager == NULL) {
        wlm_log_error("mirror-screencopy::init(): missing wlr_screencopy protocol\n");
        return;
    }

    // allocate backend context structure
    screencopy_mirror_backend_t * backend = calloc(1, sizeof (screencopy_mirror_backend_t));
    if (backend == NULL) {
        wlm_log_error("mirror-screencopy::init(): failed to allocate backend state\n");
        return;
    }

    // initialize context structure
    backend->header.do_capture = do_capture;
    backend->header.do_cleanup = do_cleanup;
    backend->header.fail_count = 0;

    backend->shm_fd = -1;
    backend->shm_size = 0;
    backend->shm_addr = NULL;
    backend->shm_pool = NULL;
    backend->shm_buffer = NULL;

    backend->screencopy_frame = NULL;

    backend->frame_width = 0;
    backend->frame_height = 0;
    backend->frame_stride = 0;
    backend->frame_format = 0;
    backend->frame_flags = 0;

    backend->state = STATE_READY;

    // set backend object as current backend
    ctx->mirror.backend = (mirror_backend_t *)backend;

    // create shm fd
    backend->shm_fd = memfd_create("wl_shm_buffer", 0);
    if (backend->shm_fd == -1) {
        wlm_log_error("mirror-screencopy::init(): failed to create shm buffer\n");
        wlm_mirror_backend_fail(ctx);
    }

    // resize shm fd to nonempty size
    backend->shm_size = 1;
    if (ftruncate(backend->shm_fd, backend->shm_size) == -1) {
        wlm_log_error("mirror-screencopy::init(): failed to resize shm buffer\n");
        wlm_mirror_backend_fail(ctx);
    }

    // map shm fd
    backend->shm_addr = mmap(NULL, backend->shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, backend->shm_fd, 0);
    if (backend->shm_addr == MAP_FAILED) {
        backend->shm_addr = NULL;
        wlm_log_error("mirror-screencopy::init(): failed to map shm buffer\n");
        wlm_mirror_backend_fail(ctx);
    }

    // create shm pool from shm fd
    backend->shm_pool = wl_shm_create_pool(ctx->wl.shm, backend->shm_fd, backend->shm_size);
    if (backend->shm_pool == NULL) {
        wlm_log_error("mirror-screencopy::init(): failed to create shm pool\n");
        wlm_mirror_backend_fail(ctx);
    }
}
