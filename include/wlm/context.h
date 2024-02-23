#ifndef WL_MIRROR_CONTEXT_H_
#define WL_MIRROR_CONTEXT_H_

#include <stdint.h>
#include <stdnoreturn.h>

#include <wlm/log.h>
#include <wlm/options.h>
#include <wlm/event/emit.h>
#include <wlm/event/loop.h>
//#include "stream.h"
#include <wlm/wayland.h>
#include <wlm/egl.h>
//#include "mirror.h"

typedef struct ctx {
    //ctx_opt_t opt;
    ctx_log_t log;
    ctx_event_loop_t event;
    //ctx_stream_t stream;
    ctx_wl_t wl;
    ctx_egl_t egl;
    //ctx_mirror_t mirror;
} ctx_t;

noreturn void wlm_exit_fail(ctx_t *);
void wlm_cleanup(ctx_t *);

#endif
