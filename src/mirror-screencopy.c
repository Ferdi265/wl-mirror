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
}

static void screencopy_frame_event_buffer_done(
    void * data, struct zwlr_screencopy_frame_v1 * frame
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;
}

static void screencopy_frame_event_linux_dmabuf(
    void * data, struct zwlr_screencopy_frame_v1 * frame,
    uint32_t format, uint32_t width, uint32_t height
) {
    (void)data;
    (void)frame;
    (void)format;
    (void)width;
    (void)height;
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
}

static void screencopy_frame_event_ready(
    void * data, struct zwlr_screencopy_frame_v1 * frame,
    uint32_t sec_hi, uint32_t sec_lo, uint32_t nsec
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;
}

static void screencopy_frame_event_failed(
    void * data, struct zwlr_screencopy_frame_v1 * frame
) {
    ctx_t * ctx = (ctx_t *)data;
    screencopy_mirror_backend_t * backend = (screencopy_mirror_backend_t *)ctx->mirror.backend;
}

static const struct zwlr_screencopy_frame_v1_listener screencopy_frame_listener = {
    .buffer = screencopy_frame_event_buffer,
    .buffer_done = screencopy_frame_event_buffer_done,
    .linux_dmabuf = screencopy_frame_event_linux_dmabuf,
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
        backend->state = STATE_WAIT_FRAME;

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
    backend->shm_pool = NULL;
    backend->shm_buffer = NULL;
    backend->screencopy_frame = NULL;

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

    backend->shm_pool = wl_shm_create_pool(ctx->wl.shm, backend->shm_fd, backend->shm_size);
    if (backend->shm_pool == NULL) {
        log_error("init_mirror_screencopy: failed to create shm pool\n");
        backend_fail(ctx);
    }
}
