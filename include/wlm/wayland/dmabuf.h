#ifndef WLM_WAYLAND_DMABUF_H_
#define WLM_WAYLAND_DMABUF_H_

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <wlm/egl.h>
#include <wlm/proto/linux-dmabuf-unstable-v1.h>

#ifdef WITH_GBM
#include <gbm.h>
#endif

typedef struct ctx ctx_t;

typedef void wlm_wayland_dmabuf_callback_t(ctx_t * ctx, bool success);

typedef struct ctx_wl_dmabuf {
#ifdef WITH_GBM
    // libgbm objects
    struct gbm_device * gbm_device;
#endif

    // callbacks
    wlm_wayland_dmabuf_callback_t * open_device_callback;
    wlm_wayland_dmabuf_callback_t * alloc_callback;

    // wp linux dmabuf objects
    struct zwp_linux_dmabuf_feedback_v1 * feedback;
    struct zwp_linux_buffer_params_v1 * buffer_params;
    struct wl_buffer * buffer;
    dmabuf_t raw_buffer;

    bool initialized;
} ctx_wl_dmabuf_t;

void wlm_wayland_dmabuf_init(ctx_t * ctx);
void wlm_wayland_dmabuf_cleanup(ctx_t * ctx);

/// Open the main GBM Device advertised by linux-dmabuf-v1
///
/// Closes any previously open device or buffers
void wlm_wayland_dmabuf_open_main_device(ctx_t * ctx, wlm_wayland_dmabuf_callback_t * cb);

/// Open the passed GBM Device
///
/// Closes any previously open device or buffers
bool wlm_wayland_dmabuf_open_device(ctx_t * ctx, dev_t device);

/// Allocate a DMA-BUF
///
/// modifiers may be NULL, in which case implicit modifiers are used.
///
/// This interface manages only one single DMA-BUF at a time.
/// Calling this function a second time without deallocating results in an error.
///
/// TODO: Allow managing multiple DMA-BUFs
void wlm_wayland_dmabuf_alloc(ctx_t * ctx, uint32_t drm_format, uint32_t width, uint32_t height, uint64_t * modifiers, size_t num_modifiers, wlm_wayland_dmabuf_callback_t * cb);

/// Deallocate the allocated DMA-BUF
///
/// TODO: Allow identifying the buffer to be deallocated
void wlm_wayland_dmabuf_dealloc(ctx_t * ctx);

/// Get the wl_buffer object for the DMA-BUF
struct wl_buffer * wlm_wayland_dmabuf_get_buffer(ctx_t * ctx);

/// Get the dmabuf_t object for the DMA-BUF
dmabuf_t * wlm_wayland_dmabuf_get_raw_buffer(ctx_t * ctx);

#endif
