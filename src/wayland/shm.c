#define _GNU_SOURCE
#include <wlm/context.h>
#include <sys/mman.h>
#include <unistd.h>

// --- helper functions ---

static bool wlm_wayland_shm_resize(ctx_t * ctx, size_t new_size) {
    if (ftruncate(ctx->wl.shmbuf.fd, new_size) == -1) {
        wlm_log_error("wayland::shm::resize(): failed to grow shm buffer\n");
        return false;
    }

#if __linux__
    void * new_addr = mremap(ctx->wl.shmbuf.addr, ctx->wl.shmbuf.size, new_size, MREMAP_MAYMOVE);
    if (new_addr == MAP_FAILED) {
        wlm_log_error("wayland::shm::resize(): failed to remap shm buffer\n");
        return false;
    }
#else
    void * new_addr = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->wl.shmbuf.fd, 0);
    if (new_addr == MAP_FAILED) {
        wlm_log_error("wayland::shm::resize(): failed to map new shm buffer\n");
        return false;
    } else {
        munmap(ctx->wl.shmbuf.addr, ctx->wl.shmbuf.size);
    }
#endif

    ctx->wl.shmbuf.size = new_size;
    ctx->wl.shmbuf.addr = new_addr;

    wl_shm_pool_resize(ctx->wl.shmbuf.pool, new_size);
    return true;
}

// --- wlm_wayland_shm_create_pool ---

bool wlm_wayland_shm_create_pool(ctx_t * ctx) {
    // check if pool already exists
    if (ctx->wl.shmbuf.pool != NULL) return true;

    if (ctx->wl.shm == NULL) {
        wlm_log_error("wayland::shm::create_pool(): missing wl_shm protocol\n");
        return false;
    }

    // create shm fd
    ctx->wl.shmbuf.fd = memfd_create("wl_shm_buffer", 0);
    if (ctx->wl.shmbuf.fd == -1) {
        wlm_log_error("wayland::shm::create_pool(): failed to create shm buffer\n");
        return false;
}

    // resize shm to nonempty size
    size_t new_size = 1;
    if (ftruncate(ctx->wl.shmbuf.fd, new_size) == -1) {
        wlm_log_error("wayland::shm::create_pool(): failed to resize shm buffer\n");
        return false;
    }
    ctx->wl.shmbuf.size = new_size;

    // map shm fd
    void * new_addr = mmap(NULL, ctx->wl.shmbuf.size, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->wl.shmbuf.fd, 0);
    if (new_addr == MAP_FAILED) {
        wlm_log_error("wayland::shm::create_pool(): failed to map shm buffer\n");
        return false;
    }
    ctx->wl.shmbuf.addr = new_addr;

    // create shm pool from shm fd
    ctx->wl.shmbuf.pool = wl_shm_create_pool(ctx->wl.shm, ctx->wl.shmbuf.fd, ctx->wl.shmbuf.size);
    if (ctx->wl.shmbuf.pool == NULL) {
        wlm_log_error("wayland::shm::create_pool(): failed to create shm pool\n");
        return false;
    }

    return true;
}

// --- wlm_wayland_shm_alloc ---

bool wlm_wayland_shm_alloc(ctx_t * ctx, uint32_t shm_format, uint32_t width, uint32_t height, uint32_t stride) {
    if (ctx->wl.shmbuf.pool == NULL && !wlm_wayland_shm_create_pool(ctx)) return false;

    if (ctx->wl.shmbuf.buffer != NULL) {
        // TODO: Allow managing multiple shared memory buffers
        wlm_log_debug(ctx, "wayland::shm::alloc(): FIXME: allow multiple buffers to be allocated\n");
        wlm_log_error("wayland::shm::alloc(): buffer already exists\n");
        return false;
    }

    // check if shmbuf needs to be resized
    size_t new_size = stride * height;
    if (new_size > ctx->wl.shmbuf.size && !wlm_wayland_shm_resize(ctx, new_size)) {
        wlm_log_error("wayland::shm::alloc(): failed to allocate shm buffer\n");
        return false;
    }

    ctx->wl.shmbuf.buffer = wl_shm_pool_create_buffer(
        ctx->wl.shmbuf.pool, 0, width, height, stride, shm_format
    );

    return true;
}

// --- wlm_wayland_shm_dealloc ---

void wlm_wayland_shm_dealloc(ctx_t * ctx) {
    if (ctx->wl.shmbuf.buffer == NULL) return;

    // TODO: Allow identifying the buffer to be deallocated
    wl_buffer_destroy(ctx->wl.shmbuf.buffer);
    ctx->wl.shmbuf.buffer = NULL;
}

// --- wlm_wayland_shm_get_buffer ---

struct wl_buffer * wlm_wayland_shm_get_buffer(ctx_t * ctx) {
    return ctx->wl.shmbuf.buffer;
}

// --- wlm_wayland_shm_get_addr ---

void * wlm_wayland_shm_get_addr(ctx_t * ctx) {
    return ctx->wl.shmbuf.addr;
}

// --- wlm_wayland_shm_init ---

void wlm_wayland_shm_init(ctx_t * ctx) {
    ctx->wl.shmbuf.fd = -1;
    ctx->wl.shmbuf.size = 0;
    ctx->wl.shmbuf.addr = NULL;
    ctx->wl.shmbuf.pool = NULL;
    ctx->wl.shmbuf.buffer = NULL;
    ctx->wl.shmbuf.initialized = true;
}

// --- wlm_wayland_shm_cleanup ---

void wlm_wayland_shm_cleanup(ctx_t * ctx) {
    if (!ctx->wl.shmbuf.initialized) return;

    wlm_log_debug(ctx, "wayland::shm::cleanup(): destroying wayland shm objects\n");

    wlm_wayland_shm_dealloc(ctx);

    if (ctx->wl.shmbuf.pool != NULL) wl_shm_pool_destroy(ctx->wl.shmbuf.pool);
    if (ctx->wl.shmbuf.addr != NULL) munmap(ctx->wl.shmbuf.addr, ctx->wl.shmbuf.size);
    if (ctx->wl.shmbuf.fd != -1) close(ctx->wl.shmbuf.fd);
}
