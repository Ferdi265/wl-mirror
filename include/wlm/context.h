#ifndef WL_MIRROR_CONTEXT_H_
#define WL_MIRROR_CONTEXT_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdnoreturn.h>

#include <wlm/log.h>
#include <wlm/options.h>
#include <wlm/event.h>
#include <wlm/stream.h>
#include <wlm/wayland.h>
#include <wlm/egl.h>
#include <wlm/mirror.h>

typedef struct ctx {
    ctx_opt_t opt;
    ctx_event_t event;
    ctx_stream_t stream;
    ctx_wl_t wl;
    ctx_egl_t egl;
    ctx_mirror_t mirror;
} ctx_t;

noreturn void wlm_exit_fail(ctx_t * ctx);
void wlm_cleanup(ctx_t * ctx);

#endif
