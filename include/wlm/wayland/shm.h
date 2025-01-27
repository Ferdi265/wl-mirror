#ifndef WL_MIRROR_WAYLAND_SHM_H_
#define WL_MIRROR_WAYLAND_SHM_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct ctx ctx_t;

typedef struct {
    uint32_t drm_format;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
} wlm_shm_buffer_t;

typedef struct ctx_wl_shm {
    // shm buffer state
    int fd;
    size_t size;
    void * addr;

    // wl shm objects
    struct wl_shm_pool * pool;
    struct wl_buffer * buffer;

    bool initialized;
} ctx_wl_shm_t;

void wlm_wayland_shm_init(ctx_t * ctx);
void wlm_wayland_shm_cleanup(ctx_t * ctx);

/// Create shm pool
///
/// Can be called before shm_alloc to initialize a pool and check if shm works.
bool wlm_wayland_shm_create_pool(ctx_t * ctx);

/// Allocate a shared memory buffer
///
/// This interface manages only one single shared memory buffer at a time.
/// Calling this function a second time results in an error.
///
/// TODO: Allow managing multiple shared memory buffers
/// TODO: properly handle wl_buffer::release event
bool wlm_wayland_shm_alloc(ctx_t * ctx, uint32_t shm_format, uint32_t width, uint32_t height, uint32_t stride);

/// Deallocate the allocated shared memory buffer
///
/// TODO: Allow identifying the buffer to be deallocated
void wlm_wayland_shm_dealloc(ctx_t * ctx);

/// Get the wl_buffer object for the shm buffer
struct wl_buffer * wlm_wayland_shm_get_buffer(ctx_t * ctx);

/// Get the mmap addr for the shm buffer
void * wlm_wayland_shm_get_addr(ctx_t * ctx);
#endif
