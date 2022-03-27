#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "context.h"
#include "mirror-screencopy.h"
#include <sys/mman.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

static void backend_cancel(screencopy_mirror_backend_t * backend) {
    log_error("mirror-screencopy::backend_cancel(): cancelling capture due to error\n");

    // destroy screencopy frame object
    zwlr_screencopy_frame_v1_destroy(backend->screencopy_frame);
    backend->screencopy_frame = NULL;
    backend->state = STATE_CANCELED;
    backend->header.fail_count++;
}

typedef struct {
    uint32_t shm_format;
    uint32_t bpp;
    GLint gl_format;
    GLint gl_type;
} shm_gl_format_t;

static const shm_gl_format_t shm_gl_formats[] = {
    {
        .shm_format = WL_SHM_FORMAT_ARGB8888,
        .bpp = 32,
        .gl_format = GL_BGRA_EXT,
        .gl_type = GL_UNSIGNED_BYTE,
    },
    {
        .shm_format = WL_SHM_FORMAT_XRGB8888,
        .bpp = 32,
        .gl_format = GL_BGRA_EXT,
        .gl_type = GL_UNSIGNED_BYTE,
    },
    {
        .shm_format = WL_SHM_FORMAT_XBGR8888,
        .bpp = 32,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_BYTE,
    },
    {
        .shm_format = WL_SHM_FORMAT_ABGR8888,
        .bpp = 32,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_BYTE,
    },
    {
        .shm_format = WL_SHM_FORMAT_BGR888,
        .bpp = 24,
        .gl_format = GL_RGB,
        .gl_type = GL_UNSIGNED_BYTE,
    },
    {
        .shm_format = WL_SHM_FORMAT_RGBX4444,
        .bpp = 16,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_SHORT_4_4_4_4,
    },
    {
        .shm_format = WL_SHM_FORMAT_RGBA4444,
        .bpp = 16,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_SHORT_4_4_4_4,
    },
    {
        .shm_format = WL_SHM_FORMAT_RGBX5551,
        .bpp = 16,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_SHORT_5_5_5_1,
    },
    {
        .shm_format = WL_SHM_FORMAT_RGBA5551,
        .bpp = 16,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_SHORT_5_5_5_1,
    },
    {
        .shm_format = WL_SHM_FORMAT_RGB565,
        .bpp = 16,
        .gl_format = GL_RGB,
        .gl_type = GL_UNSIGNED_SHORT_5_6_5,
    },
    {
        .shm_format = WL_SHM_FORMAT_XBGR2101010,
        .bpp = 32,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_INT_2_10_10_10_REV_EXT,
    },
    {
        .shm_format = WL_SHM_FORMAT_ABGR2101010,
        .bpp = 32,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_INT_2_10_10_10_REV_EXT,
    },
    {
        .shm_format = WL_SHM_FORMAT_XBGR16161616F,
        .bpp = 64,
        .gl_format = GL_RGBA,
        .gl_type = GL_HALF_FLOAT_OES,
    },
    {
        .shm_format = WL_SHM_FORMAT_ABGR16161616F,
        .bpp = 64,
        .gl_format = GL_RGBA,
        .gl_type = GL_HALF_FLOAT_OES,
    },
    {
        .shm_format = -1U,
        .bpp = -1U,
        .gl_format = -1,
        .gl_type = -1
    }
};

static const shm_gl_format_t * shm_gl_format_from_shm(uint32_t shm_format) {
    const shm_gl_format_t * format = shm_gl_formats;
    while (format->shm_format != -1U) {
        if (format->shm_format == shm_format) {
            return format;
        }

        format++;
    }

    return NULL;
}

// --- screencopy_frame event handlers ---

static void on_buffer(
    void * data, struct zwlr_screencopy_frame_v1 * frame,
    uint32_t format, uint32_t width, uint32_t height, uint32_t stride
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    log_debug(ctx, "mirror-screencopy::on_buffer(): received buffer offer for %dx%d+%d frame\n", width, height, stride);
    if (backend->state != STATE_WAIT_BUFFER) {
        log_error("mirror-screencopy::on_buffer(): got buffer event while in state %d\n", backend->state);
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
            log_error("mirror-screencopy::on_buffer(): failed to grow shm buffer\n");
            backend_cancel(backend);
            return;
        }

        void * new_addr = mremap(backend->shm_addr, backend->shm_size, new_size, MREMAP_MAYMOVE);
        if (new_addr == MAP_FAILED) {
            log_error("mirror-screencopy::on_buffer(): failed to remap shm buffer\n");
            backend_cancel(backend);
            return;
        }

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
            log_error("mirror-screencopy::on_buffer(): failed to create wl_buffer\n");
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

    log_debug(ctx, "mirror-screencopy::on_buffer_done(): received buffer done event\n");
    if (backend->state != STATE_WAIT_BUFFER_DONE) {
        log_error("mirror-screencopy::on_buffer_done(): received buffer_done without supported buffer offer\n");
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

    log_debug(ctx, "mirror-screencopy::on_flags(): received flags event\n");
    if (backend->state != STATE_WAIT_FLAGS) {
        log_error("mirror-screencopy::on_flags(): received unexpected flags event\n");
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
        log_debug(ctx, "mirror-screencopy::on_ready(): received ready event with");
        fprintf(stderr, "width: %d, height: %d, stride: %d, format: %c%c%c%c\n",
            backend->frame_width, backend->frame_height,
            backend->frame_stride,
            (backend->frame_format >> 24) & 0xff,
            (backend->frame_format >> 16) & 0xff,
            (backend->frame_format >> 8) & 0xff,
            (backend->frame_format >> 0) & 0xff
        );
    }

    // find correct texture format
    const shm_gl_format_t * format = shm_gl_format_from_shm(backend->frame_format);
    if (format == NULL) {
        log_error("mirror-screencopy::on_ready(): failed to find GL format for shm format\n");
        backend_fail(ctx);
        return;
    }

    // store frame data into texture
    glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, backend->frame_stride / (format->bpp / 8));
    glTexImage2D(GL_TEXTURE_2D,
        0, format->gl_format, backend->frame_width, backend->frame_height,
        0, format->gl_format, format->gl_type, backend->shm_addr
    );
    glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);
    ctx->egl.texture_initialized = true;

    // set buffer flags
    bool invert_y = backend->frame_flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT;
    if (ctx->mirror.invert_y != invert_y) {
        ctx->mirror.invert_y = invert_y;
        update_uniforms(ctx);
    }

    // set texture size and aspect ratio only if changed
    if (backend->frame_width != ctx->egl.width || backend->frame_height != ctx->egl.height) {
        ctx->egl.width = backend->frame_width;
        ctx->egl.height = backend->frame_height;
        resize_viewport(ctx);
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

    log_debug(ctx, "mirror-screencopy::on_failed(): received cancel event\n");

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
        backend->screencopy_frame = zwlr_screencopy_manager_v1_capture_output(
            ctx->wl.screencopy_manager, ctx->opt.show_cursor, ctx->mirror.current_target->output
        );
        if (backend->screencopy_frame == NULL) {
            log_error("do_capture: failed to create wlr_screencopy_frame\n");
            backend_fail(ctx);
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

    log_debug(ctx, "mirror-screencopy::do_cleanup(): destroying mirror-screencopy objects\n");

    if (backend->screencopy_frame != NULL) zwlr_screencopy_frame_v1_destroy(backend->screencopy_frame);
    if (backend->shm_buffer != NULL) wl_buffer_destroy(backend->shm_buffer);
    if (backend->shm_pool != NULL) wl_shm_pool_destroy(backend->shm_pool);
    if (backend->shm_addr != NULL) munmap(backend->shm_addr, backend->shm_size);
    if (backend->shm_fd != -1) close(backend->shm_fd);

    free(backend);
    ctx->mirror.backend = NULL;
}

// --- init_mirror_screencopy ---

void init_mirror_screencopy(ctx_t * ctx) {
    // check for required protocols
    if (ctx->wl.shm == NULL) {
        log_error("mirror-screencopy::init(): missing wl_shm protocol\n");
        return;
    } else if (ctx->wl.screencopy_manager == NULL) {
        log_error("mirror-screencopy::init(): missing wlr_screencopy_manager protocol\n");
        return;
    }

    // allocate backend context structure
    screencopy_mirror_backend_t * backend = malloc(sizeof (screencopy_mirror_backend_t));
    if (backend == NULL) {
        log_error("mirror-screencopy::init(): failed to allocate backend state\n");
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

    // destroy EGLImage if previous backend created it
    // - this backend does not need EGLImages
    if (ctx->mirror.frame_image != EGL_NO_IMAGE) {
        eglDestroyImage(ctx->egl.display, ctx->mirror.frame_image);
    }

    // create shm fd
    backend->shm_fd = memfd_create("wl_shm_buffer", 0);
    if (backend->shm_fd == -1) {
        log_error("mirror-screencopy::init(): failed to create shm buffer\n");
        backend_fail(ctx);
    }

    // resize shm fd to nonempty size
    backend->shm_size = 1;
    if (ftruncate(backend->shm_fd, backend->shm_size) == -1) {
        log_error("mirror-screencopy::init(): failed to resize shm buffer\n");
        backend_fail(ctx);
    }

    // map shm fd
    backend->shm_addr = mmap(NULL, backend->shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, backend->shm_fd, 0);
    if (backend->shm_addr == MAP_FAILED) {
        backend->shm_addr = NULL;
        log_error("mirror-screencopy::init(): failed to map shm buffer\n");
        backend_fail(ctx);
    }

    // create shm pool from shm fd
    backend->shm_pool = wl_shm_create_pool(ctx->wl.shm, backend->shm_fd, backend->shm_size);
    if (backend->shm_pool == NULL) {
        log_error("mirror-screencopy::init(): failed to create shm pool\n");
        backend_fail(ctx);
    }
}
