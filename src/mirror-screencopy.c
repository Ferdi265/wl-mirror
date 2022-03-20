#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "context.h"
#include "mirror-screencopy.h"
#include <sys/mman.h>

// --- screencopy_frame event handlers ---

static void screencopy_frame_event_buffer(
    void * data, struct zwlr_screencopy_frame_v1 * frame,
    uint32_t format, uint32_t width, uint32_t height, uint32_t stride
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    log_debug(ctx, "screencopy_frame: received buffer offer for %dx%d+%d frame\n", width, height, stride);
    if (backend->state != STATE_WAIT_BUFFER) {
        log_error("screencopy_frame: got buffer event while in state %d\n", backend->state);
        backend_fail(ctx);
        return;
    }

    backend->frame_info.width = width;
    backend->frame_info.height = height;
    backend->frame_info.stride = stride;
    backend->frame_info.format = format;

    backend->state = STATE_WAIT_BUFFER_DONE;

    (void)frame;
}

static void screencopy_frame_event_linux_dmabuf(
    void * data, struct zwlr_screencopy_frame_v1 * frame,
    uint32_t format, uint32_t width, uint32_t height
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    log_debug(ctx, "screencopy_frame: received dmabuf offer for %dx%d frame\n", width, height);
    log_debug(ctx, "screencopy_frame: dmabuf buffers are not yet supported\n");

    (void)backend;
    (void)frame;
    (void)format;
}

static void screencopy_frame_event_buffer_done(
    void * data, struct zwlr_screencopy_frame_v1 * frame
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    log_debug(ctx, "screencopy_frame: received buffer done event\n");
    if (backend->state != STATE_WAIT_BUFFER_DONE) {
        log_error("screencopy_frame: received buffer_done without supported buffer offer\n");
        backend_fail(ctx);
        return;
    }

    size_t new_size = backend->frame_info.stride * backend->frame_info.height;
    if (new_size > backend->shm_size) {
        if (backend->shm_buffer != NULL) {
            wl_buffer_destroy(backend->shm_buffer);
            backend->shm_buffer = NULL;
        }

        if (ftruncate(backend->shm_fd, new_size) == -1) {
            log_error("screencopy_frame: failed to grow shm buffer\n");
            backend_fail(ctx);
            return;
        }

        void * new_addr = mremap(backend->shm_addr, backend->shm_size, new_size, MREMAP_MAYMOVE);
        if (new_addr == MAP_FAILED) {
            log_error("screencopy_frame: failed to remap shm buffer\n");
            backend_fail(ctx);
            return;
        }

        backend->shm_addr = new_addr;
        backend->shm_size = new_size;

        wl_shm_pool_resize(backend->shm_pool, new_size);
    }

    bool new_buffer_needed =
        backend->buffer_info.width != backend->frame_info.width ||
        backend->buffer_info.height != backend->frame_info.height ||
        backend->buffer_info.stride != backend->frame_info.stride ||
        backend->buffer_info.format != backend->frame_info.format;

    if (backend->shm_buffer != NULL && new_buffer_needed) {
        wl_buffer_destroy(backend->shm_buffer);
        backend->shm_buffer = NULL;
    }

    if (backend->shm_buffer == NULL) {
        backend->shm_buffer = wl_shm_pool_create_buffer(
            backend->shm_pool, 0,
            backend->frame_info.width, backend->frame_info.height,
            backend->frame_info.stride, backend->frame_info.format
        );
        if (backend->shm_buffer == NULL) {
            log_error("screencopy_frame: failed to create wl_buffer\n");
            backend_fail(ctx);
            return;
        }
    }

    backend->state = STATE_WAIT_FLAGS;
    zwlr_screencopy_frame_v1_copy(backend->screencopy_frame, backend->shm_buffer);

    (void)frame;
}

static void screencopy_frame_event_damage(
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

static void screencopy_frame_event_flags(
    void * data, struct zwlr_screencopy_frame_v1 * frame,
    uint32_t flags
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    log_debug(ctx, "screencopy_frame: received flags event\n");
    if (backend->state != STATE_WAIT_FLAGS) {
        log_error("screencopy_frame: received unexpected flags event\n");
        backend_fail(ctx);
        return;
    }

    backend->frame_flags = flags;
    backend->state = STATE_WAIT_READY;

    (void)frame;
}

static void screencopy_frame_event_ready(
    void * data, struct zwlr_screencopy_frame_v1 * frame,
    uint32_t sec_hi, uint32_t sec_lo, uint32_t nsec
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    log_debug(ctx, "screencopy_frame: received ready event\n");
    log_debug(ctx, "width: %d, height: %d, stride: %d, format: %x\n",
        backend->frame_info.width, backend->frame_info.height,
        backend->frame_info.stride, backend->frame_info.format
    );

    backend_fail(ctx);

    // TODO: somehow create OpenGL texture
    // (lots of cleanup needed elsewhere too)
    // (EGLImage might be able to be removed from common mirror and moved into dmabuf)

    zwlr_screencopy_frame_v1_destroy(backend->screencopy_frame);
    backend->screencopy_frame = NULL;
    backend->state = STATE_READY;
    backend->header.fail_count = 0;

    (void)frame;
}

static void screencopy_frame_event_failed(
    void * data, struct zwlr_screencopy_frame_v1 * frame
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    backend->header.fail_count++;
    zwlr_screencopy_frame_v1_destroy(backend->screencopy_frame);
    backend->screencopy_frame = NULL;
    backend->state = STATE_CANCELED;

    (void)frame;
}

static const struct zwlr_screencopy_frame_v1_listener screencopy_frame_listener = {
    .buffer = screencopy_frame_event_buffer,
    .linux_dmabuf = screencopy_frame_event_linux_dmabuf,
    .buffer_done = screencopy_frame_event_buffer_done,
    .damage = screencopy_frame_event_damage,
    .flags = screencopy_frame_event_flags,
    .ready = screencopy_frame_event_ready,
    .failed = screencopy_frame_event_failed
};

// --- backend event handlers ---

static void mirror_screencopy_on_frame(ctx_t * ctx) {
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    if (backend->state == STATE_READY || backend->state == STATE_CANCELED) {
        log_debug(ctx, "mirror_screencopy_on_frame: clearing screencopy_frame state\n");

        buffer_info_t zero_buffer = {
            .width = 0,
            .height = 0,
            .stride = 0,
            .format = 0
        };
        backend->frame_info = zero_buffer;
        backend->frame_flags = 0;
        backend->state = STATE_WAIT_BUFFER;

        backend->screencopy_frame = zwlr_screencopy_manager_v1_capture_output(
            ctx->wl.screencopy_manager, ctx->opt.show_cursor, ctx->mirror.current_target->output
        );
        if (backend->screencopy_frame == NULL) {
            log_error("mirror_screencopy_on_frame: failed to create wlr_screencopy_frame\n");
            backend_fail(ctx);
        }

        log_debug(ctx, "mirror_screencopy_on_frame: adding screencopy_frame event listener\n");
        zwlr_screencopy_frame_v1_add_listener(backend->screencopy_frame, &screencopy_frame_listener, (void *)ctx);
    }
}

static void mirror_screencopy_on_cleanup(ctx_t * ctx) {
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;

    log_debug(ctx, "mirror_screencopy_on_cleanup: destroying mirror-screencopy objects\n");

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
    if (ctx->wl.shm == NULL) {
        log_error("init_mirror_screencopy: missing wl_shm protocol\n");
        return;
    } else if (ctx->wl.screencopy_manager == NULL) {
        log_error("init_mirror_screencopy: missing wlr_screencopy_manager protocol\n");
        return;
    }

    screencopy_mirror_backend_t * backend = malloc(sizeof (screencopy_mirror_backend_t));
    if (backend == NULL) {
        log_error("init_mirror_screencopy: failed to allocate backend state\n");
        return;
    }

    backend->header.on_frame = mirror_screencopy_on_frame;
    backend->header.on_cleanup = mirror_screencopy_on_cleanup;
    backend->header.fail_count = 0;

    backend->shm_fd = -1;
    backend->shm_size = 0;
    backend->shm_addr = NULL;
    backend->shm_pool = NULL;
    backend->shm_buffer = NULL;

    backend->screencopy_frame = NULL;

    buffer_info_t zero_buffer = {
        .width = 0,
        .height = 0,
        .stride = 0,
        .format = 0
    };
    backend->buffer_info = zero_buffer;
    backend->frame_info = zero_buffer;
    backend->frame_flags = 0;

    backend->state = STATE_READY;

    ctx->mirror.backend = (mirror_backend_t *)backend;

    backend->shm_fd = memfd_create("wl_shm_buffer", 0);
    if (backend->shm_fd == -1) {
        log_error("init_mirror_screencopy: failed to create shm buffer\n");
        backend_fail(ctx);
    }

    backend->shm_size = 1;
    if (ftruncate(backend->shm_fd, backend->shm_size) == -1) {
        log_error("init_mirror_screencopy: failed to resize shm buffer\n");
        backend_fail(ctx);
    }

    backend->shm_addr = mmap(NULL, backend->shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, backend->shm_fd, 0);
    if (backend->shm_addr == MAP_FAILED) {
        backend->shm_addr = NULL;
        log_error("init_mirror_screencopy: failed to map shm buffer\n");
        backend_fail(ctx);
    }

    backend->shm_pool = wl_shm_create_pool(ctx->wl.shm, backend->shm_fd, backend->shm_size);
    if (backend->shm_pool == NULL) {
        log_error("init_mirror_screencopy: failed to create shm pool\n");
        backend_fail(ctx);
    }
}
