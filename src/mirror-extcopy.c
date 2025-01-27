#include <wlm/mirror-extcopy.h>
#include <wlm/context.h>
#include <stdlib.h>

static void extcopy_frame_cleanup(extcopy_mirror_backend_t * backend) {
    (void)backend;
}

static void backend_cancel(extcopy_mirror_backend_t * backend) {
    wlm_log_error("mirror-extcopy::backend_cancel(): cancelling capture due to error\n");

    extcopy_frame_cleanup(backend);
    backend->state = STATE_CANCELED;
    backend->header.fail_count++;
}

// --- backend event handlers ---

static void do_capture(ctx_t * ctx) {
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    if (backend->state == STATE_INIT || backend->state == STATE_CANCELED) {
        // TODO: create session
        // set state to WAIT_BUFFER_INFO
        // create frame after receiving buffer info
        // then transition to WAIT_READY
    } else if (backend->state == STATE_READY) {
        // TODO: create frame
        // set state to WAIT_READY
        // draw frame in frame ready callback
        // then transition to READY
    }
}

static void do_cleanup(ctx_t * ctx) {
    extcopy_mirror_backend_t * backend = (extcopy_mirror_backend_t *)ctx->mirror.backend;

    wlm_log_debug(ctx, "mirror-extcopy::do_cleanup(): destroying mirror-extcopy objects\n");
    extcopy_frame_cleanup(backend);

    free(backend);
    ctx->mirror.backend = NULL;
}

// --- wlm_mirror_extcopy_init ---

void wlm_mirror_extcopy_init(ctx_t * ctx) {
    // check for required protocols
    if (ctx->wl.copy_capture_manager == NULL) {
        wlm_log_error("mirror-extcopy::init(): missing ext_image_copy_capture protocol\n");
        return;
    }

    if (ctx->wl.output_capture_source_manager == NULL) {
        wlm_log_error("mirror-extcopy::init(): missing ext_output_image_capture_source_manager protocol\n");
        return;
    }

    // allocate backend context structure
    extcopy_mirror_backend_t * backend = calloc(1, sizeof (extcopy_mirror_backend_t));
    if (backend == NULL) {
        wlm_log_error("mirror-extcopy::init(): failed to allocate backend state\n");
        return;
    }

    backend->header.do_capture = do_capture;
    backend->header.do_cleanup = do_cleanup;
    backend->header.fail_count = 0;

    backend->capture_source = NULL;
    backend->capture_session = NULL;
    backend->capture_frame = NULL;

#ifdef WITH_GBM
    backend->gbm_device = NULL;
    backend->gbm_dmabuf = NULL;
#endif

    backend->state = STATE_READY;

    // set backend object as current backend
    ctx->mirror.backend = (mirror_backend_t *)backend;
}
