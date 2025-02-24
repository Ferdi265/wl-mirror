#include "wlm/proto/linux-dmabuf-unstable-v1.h"
#include <wlm/context.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef WITH_GBM
#include <xf86drm.h>
#endif

// --- linux_dmabuf_feedback event handlers ---

static void on_linux_dmabuf_feedback_main_device(void * data, struct zwp_linux_dmabuf_feedback_v1 * feedback, struct wl_array * device) {
    ctx_t * ctx = (ctx_t *)data;
    wlm_log_debug(ctx, "wayland::dmabuf::on_feedback_main_device(): opening main device\n");

    if (device->size != sizeof (dev_t)) {
        wlm_log_error("array size mismatch: %zd != %zd\n", device->size, sizeof (dev_t));
        wlm_exit_fail(ctx);
    }

    dev_t device_id;
    memcpy(&device_id, device->data, sizeof (dev_t));

    if (ctx->wl.dmabuf.open_device_callback != NULL) {
        bool success = wlm_wayland_dmabuf_open_device(ctx, device_id);
        ctx->wl.dmabuf.open_device_callback(ctx, success);
        ctx->wl.dmabuf.open_device_callback = NULL;
    }

    (void)feedback;
}

static void on_linux_dmabuf_feedback_format_table(void * data, struct zwp_linux_dmabuf_feedback_v1 * feedback, int fd, uint32_t size) {
    (void)data;
    (void)feedback;
    (void)fd;
    (void)size;
}

static void on_linux_dmabuf_feedback_tranche_target_device(void * data, struct zwp_linux_dmabuf_feedback_v1 * feedback, struct wl_array * device) {
    (void)data;
    (void)feedback;
    (void)device;
}

static void on_linux_dmabuf_feedback_tranche_flags(void * data, struct zwp_linux_dmabuf_feedback_v1 * feedback, enum zwp_linux_dmabuf_feedback_v1_tranche_flags flags) {
    (void)data;
    (void)feedback;
    (void)flags;
}

static void on_linux_dmabuf_feedback_tranche_formats(void * data, struct zwp_linux_dmabuf_feedback_v1 * feedback, struct wl_array * indices) {
    (void)data;
    (void)feedback;
    (void)indices;
}

static void on_linux_dmabuf_feedback_tranche_done(void * data, struct zwp_linux_dmabuf_feedback_v1 * feedback) {
    (void)data;
    (void)feedback;
}

static void on_linux_dmabuf_feedback_done(void * data, struct zwp_linux_dmabuf_feedback_v1 * feedback) {
    ctx_t * ctx = (ctx_t *)data;
    wlm_log_debug(ctx, "wayland::dmabuf::feedback_done(): feedback done\n");

    (void)feedback;
}


static const struct zwp_linux_dmabuf_feedback_v1_listener linux_dmabuf_feedback_listener = {
    .main_device = on_linux_dmabuf_feedback_main_device,
    .format_table = on_linux_dmabuf_feedback_format_table,
    .tranche_target_device = on_linux_dmabuf_feedback_tranche_target_device,
    .tranche_flags = on_linux_dmabuf_feedback_tranche_flags,
    .tranche_formats = on_linux_dmabuf_feedback_tranche_formats,
    .tranche_done = on_linux_dmabuf_feedback_tranche_done,
    .done = on_linux_dmabuf_feedback_done
};

// --- linux_buffer_params event handlers ---

static void on_linux_buffer_params_created(void * data, struct zwp_linux_buffer_params_v1 * buffer_params, struct wl_buffer * buffer) {
    ctx_t * ctx = (ctx_t *)data;

    zwp_linux_buffer_params_v1_destroy(buffer_params);
    if (ctx->wl.dmabuf.buffer_params == buffer_params) {
        ctx->wl.dmabuf.buffer_params = NULL;
    } else {
        wlm_log_debug(ctx, "wayland::dmabuf::on_linux_buffer_params_created(): received stale DMA-BUF creation event\n");
        wl_buffer_destroy(buffer);
        return;
    }

    bool success = true;
    if (ctx->wl.dmabuf.buffer != NULL) {
        wlm_log_error("wayland::dmabuf::on_linux_buffer_params_created(): buffer already exists\n");
        wl_buffer_destroy(buffer);
        success = false;
    } else {
        wlm_log_debug(ctx, "wayland::dmabuf::on_linux_buffer_params_created(): allocation succeeded\n");
        ctx->wl.dmabuf.buffer = buffer;
    }

    if (ctx->wl.dmabuf.alloc_callback != NULL) {
        ctx->wl.dmabuf.alloc_callback(ctx, success);
        ctx->wl.dmabuf.alloc_callback = NULL;
    }
}

static void on_linux_buffer_params_failed(void * data, struct zwp_linux_buffer_params_v1 * buffer_params) {
    ctx_t * ctx = (ctx_t *)data;

    zwp_linux_buffer_params_v1_destroy(buffer_params);
    if (ctx->wl.dmabuf.buffer_params == buffer_params) {
        ctx->wl.dmabuf.buffer_params = NULL;
    } else {
        wlm_log_debug(ctx, "wayland::dmabuf::on_linux_buffer_params_failed(): received stale DMA-BUF failure event\n");
        return;
    }

    if (ctx->wl.dmabuf.alloc_callback != NULL) {
        wlm_log_error("wayland::dmabuf::on_linux_buffer_params_failed(): allocation failed\n");
        ctx->wl.dmabuf.alloc_callback(ctx, false);
        ctx->wl.dmabuf.alloc_callback = NULL;
    }
}

static const struct zwp_linux_buffer_params_v1_listener linux_buffer_params_listener = {
    .created = on_linux_buffer_params_created,
    .failed = on_linux_buffer_params_failed,
};

// --- wlm_wayland_dmabuf_open_main_device ---

void wlm_wayland_dmabuf_open_main_device(ctx_t * ctx, wlm_wayland_dmabuf_callback_t * cb) {
#ifdef WITH_GBM
    if (ctx->wl.dmabuf.gbm_device != NULL) {
        gbm_device_destroy(ctx->wl.dmabuf.gbm_device);
        ctx->wl.dmabuf.gbm_device = NULL;
    }

    if (ctx->wl.dmabuf.feedback != NULL) {
        zwp_linux_dmabuf_feedback_v1_destroy(ctx->wl.dmabuf.feedback);
        ctx->wl.dmabuf.feedback = NULL;
    }

    if (ctx->wl.linux_dmabuf == NULL) {
        wlm_log_error("wayland::shm::create_pool(): missing linux_dmabuf protocol\n");
        cb(ctx, false);
    }

    ctx->wl.dmabuf.open_device_callback = cb;
    ctx->wl.dmabuf.feedback = zwp_linux_dmabuf_v1_get_default_feedback(ctx->wl.linux_dmabuf);
    zwp_linux_dmabuf_feedback_v1_add_listener(ctx->wl.dmabuf.feedback, &linux_dmabuf_feedback_listener, (void *)ctx);
#else
    wlm_log_error("wayland::dmabuf::open_device(): need libGBM for dmabuf allocation\n");
    cb(ctx, false);
#endif
}

// --- wlm_wayland_dmabuf_open_device ---

bool wlm_wayland_dmabuf_open_device(ctx_t * ctx, dev_t device) {
#ifdef WITH_GBM
    if (ctx->wl.dmabuf.gbm_device != NULL) {
        gbm_device_destroy(ctx->wl.dmabuf.gbm_device);
        ctx->wl.dmabuf.gbm_device = NULL;
    }

    if (ctx->wl.dmabuf.feedback != NULL) {
        zwp_linux_dmabuf_feedback_v1_destroy(ctx->wl.dmabuf.feedback);
        ctx->wl.dmabuf.feedback = NULL;
    }

    drmDevice * drm_device = NULL;
    if (drmGetDeviceFromDevId(device, 0, &drm_device) != 0) {
        wlm_log_error("wayland::dmabuf::open_device(): failed to open drm device\n");
        return false;
    }

    char * node = NULL;
    if (drm_device->available_nodes & (1 << DRM_NODE_RENDER)) {
        node = drm_device->nodes[DRM_NODE_RENDER];
    } else if (drm_device->available_nodes & (1 << DRM_NODE_PRIMARY)) {
        node = drm_device->nodes[DRM_NODE_PRIMARY];
    } else {
        wlm_log_error("wayland::dmabuf::open_device(): failed to find drm node to open\n");
        drmFreeDevice(&drm_device);
        return false;
    }

    int fd = open(node, O_RDWR | O_CLOEXEC);
    if (fd == -1) {
        wlm_log_error("wayland::dmabuf::open_device(): failed to open drm node\n");
        drmFreeDevice(&drm_device);
        return false;
    }

    drmFreeDevice(&drm_device);

    wlm_log_debug(ctx, "wayland::dmabuf::open_device(): opening gbm device\n");
    ctx->wl.dmabuf.gbm_device = gbm_create_device(fd);
    if (ctx->wl.dmabuf.gbm_device == NULL) {
        wlm_log_error("wayland::dmabuf::open_device(): failed to open gbm device\n");
        return false;
    }

    return true;
#else
    wlm_log_error("wayland::dmabuf::open_device(): need libGBM for dmabuf allocation\n");
    return false;
#endif
}

// --- wlm_wayland_dmabuf_alloc ---

void wlm_wayland_dmabuf_alloc(ctx_t * ctx, uint32_t drm_format, uint32_t width, uint32_t height, uint64_t * modifiers, size_t num_modifiers, wlm_wayland_dmabuf_callback_t * cb) {
#ifdef WITH_GBM
    if (ctx->wl.dmabuf.buffer != NULL) {
        wlm_log_error("wayland::dmabuf::alloc(): already allocated\n");
        cb(ctx, false);
        return;
    }

    if (ctx->wl.dmabuf.gbm_device == NULL) {
        wlm_log_error("wayland::dmabuf::alloc(): no gbm device\n");
        cb(ctx, false);
        return;
    }

    // NOTE: old buffer params object destroys itself on success/failure
    ctx->wl.dmabuf.buffer_params = NULL;

    struct gbm_bo * dmabuf_bo = NULL;
    if (modifiers == NULL) {
        dmabuf_bo = gbm_bo_create(ctx->wl.dmabuf.gbm_device, width, height, drm_format, GBM_BO_USE_RENDERING);
    } else {
        dmabuf_bo = gbm_bo_create_with_modifiers2(ctx->wl.dmabuf.gbm_device, width, height, drm_format, modifiers, num_modifiers, GBM_BO_USE_RENDERING);
    }

    if (dmabuf_bo == NULL) {
        wlm_log_error("wayland::dmabuf::alloc(): failed to create gbm bo\n");
        cb(ctx, false);
        return;
    }

    // export gbm bo to raw dmabuf
    size_t num_planes = gbm_bo_get_plane_count(dmabuf_bo);
    uint64_t modifier = gbm_bo_get_modifier(dmabuf_bo);

    // allocate dmabuf plane arrays
    int * fds = calloc(num_planes, sizeof (int));
    uint32_t * offsets = calloc(num_planes, sizeof (uint32_t));
    uint32_t * strides = calloc(num_planes, sizeof (uint32_t));
    if (fds == NULL || offsets == NULL || strides == NULL) {
        wlm_log_error("wayland::dmabuf::alloc(): failed to allocate dmabuf plane arrays\n");
        gbm_bo_destroy(dmabuf_bo);
        free(fds);
        free(offsets);
        free(strides);
        cb(ctx, false);
    }

    // fill dmabuf plane arrays
    ctx->wl.dmabuf.raw_buffer.width = width;
    ctx->wl.dmabuf.raw_buffer.height = height;
    ctx->wl.dmabuf.raw_buffer.drm_format = drm_format;
    ctx->wl.dmabuf.raw_buffer.planes = num_planes;
    ctx->wl.dmabuf.raw_buffer.fds = fds;
    ctx->wl.dmabuf.raw_buffer.offsets = offsets;
    ctx->wl.dmabuf.raw_buffer.strides = strides;
    ctx->wl.dmabuf.raw_buffer.modifier = modifier;
    wlm_log_debug(ctx, "wayland::dmabuf::alloc(): allocated dmabuf with format=%x, size=%dx%d, modifier=%zx, planes=%zd\n", drm_format, width, height, modifier, num_planes);
    for (size_t i = 0; i < num_planes; i++) {
        fds[i] = gbm_bo_get_fd_for_plane(dmabuf_bo, i);
        offsets[i] = gbm_bo_get_offset(dmabuf_bo, i);
        strides[i] = gbm_bo_get_stride_for_plane(dmabuf_bo, i);
        wlm_log_debug(ctx, "wayland::dmabuf::alloc(): plane[%zd]: fd=%d, offset=%x, stride=%x\n", i, fds[i], offsets[i], strides[i]);
    }
    gbm_bo_destroy(dmabuf_bo);

    // create dmabuf wl_buffer
    ctx->wl.dmabuf.alloc_callback = cb;
    ctx->wl.dmabuf.buffer_params = zwp_linux_dmabuf_v1_create_params(ctx->wl.linux_dmabuf);
    zwp_linux_buffer_params_v1_add_listener(ctx->wl.dmabuf.buffer_params, &linux_buffer_params_listener, (void *)ctx);

    for (size_t i = 0; i < num_planes; i++) {
        zwp_linux_buffer_params_v1_add(ctx->wl.dmabuf.buffer_params, fds[i], i, offsets[i], strides[i], modifier >> 32, modifier);
    }

    zwp_linux_buffer_params_v1_create(ctx->wl.dmabuf.buffer_params, width, height, drm_format, 0);
#else
    wlm_log_error("wayland::dmabuf::open_device(): need libGBM for dmabuf allocation\n");
    cb(ctx, false);
#endif
}

// --- wlm_wayland_dmabuf_dealloc ---

void wlm_wayland_dmabuf_dealloc(ctx_t * ctx) {
    // NOTE: old buffer params object destroys itself on success/failure
    ctx->wl.dmabuf.buffer_params = NULL;

    if (ctx->wl.dmabuf.buffer != NULL) wl_buffer_destroy(ctx->wl.dmabuf.buffer);
    ctx->wl.dmabuf.buffer = NULL;

    if (ctx->wl.dmabuf.raw_buffer.planes == 0) return;

    // close dmabuf file descriptors
    for (unsigned int i = 0; i < ctx->wl.dmabuf.raw_buffer.planes; i++) {
        if (ctx->wl.dmabuf.raw_buffer.fds[i] != -1) close(ctx->wl.dmabuf.raw_buffer.fds[i]);
    }
    // free dmabuf arrays
    free(ctx->wl.dmabuf.raw_buffer.fds);
    free(ctx->wl.dmabuf.raw_buffer.offsets);
    free(ctx->wl.dmabuf.raw_buffer.strides);

    // reset dmabuf variables
    ctx->wl.dmabuf.raw_buffer.width = 0;
    ctx->wl.dmabuf.raw_buffer.height = 0;
    ctx->wl.dmabuf.raw_buffer.drm_format = 0;
    ctx->wl.dmabuf.raw_buffer.planes = 0;
    ctx->wl.dmabuf.raw_buffer.fds = NULL;
    ctx->wl.dmabuf.raw_buffer.offsets = NULL;
    ctx->wl.dmabuf.raw_buffer.strides = NULL;
    ctx->wl.dmabuf.raw_buffer.modifier = 0;
}

// --- wlm_wayland_dmabuf_get_buffer ---

struct wl_buffer * wlm_wayland_dmabuf_get_buffer(ctx_t * ctx) {
    return ctx->wl.dmabuf.buffer;
}

// --- wlm_wayland_dmabuf_get_raw_buffer ---

dmabuf_t * wlm_wayland_dmabuf_get_raw_buffer(ctx_t * ctx) {
    if (ctx->wl.dmabuf.buffer == NULL) return NULL;
    return &ctx->wl.dmabuf.raw_buffer;
}

// --- wlm_wayland_dmabuf_init ---

void wlm_wayland_dmabuf_init(ctx_t * ctx) {
#ifdef WITH_GBM
    ctx->wl.dmabuf.gbm_device = NULL;
#endif

    ctx->wl.dmabuf.open_device_callback = NULL;
    ctx->wl.dmabuf.alloc_callback = NULL;
    ctx->wl.dmabuf.feedback = NULL;
    ctx->wl.dmabuf.buffer_params = NULL;
    ctx->wl.dmabuf.buffer = NULL;
    ctx->wl.dmabuf.raw_buffer.width = 0;
    ctx->wl.dmabuf.raw_buffer.height = 0;
    ctx->wl.dmabuf.raw_buffer.drm_format = 0;
    ctx->wl.dmabuf.raw_buffer.planes = 0;
    ctx->wl.dmabuf.raw_buffer.fds = NULL;
    ctx->wl.dmabuf.raw_buffer.offsets = NULL;
    ctx->wl.dmabuf.raw_buffer.strides = NULL;
    ctx->wl.dmabuf.raw_buffer.modifier = 0;
    ctx->wl.dmabuf.initialized = true;
}

// --- wlm_wayland_dmabuf_cleanup ---

void wlm_wayland_dmabuf_cleanup(ctx_t * ctx) {
    if (!ctx->wl.dmabuf.initialized) return;

    wlm_wayland_dmabuf_dealloc(ctx);

#ifdef WITH_GBM
    if (ctx->wl.dmabuf.gbm_device != NULL) gbm_device_destroy(ctx->wl.dmabuf.gbm_device);
#endif

    if (ctx->wl.dmabuf.feedback != NULL) zwp_linux_dmabuf_feedback_v1_destroy(ctx->wl.dmabuf.feedback);
}
