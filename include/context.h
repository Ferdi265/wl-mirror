#ifndef WL_MIRROR_CONTEXT_H_
#define WL_MIRROR_CONTEXT_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdnoreturn.h>

#include "log.h"
#include "options.h"
#include "wayland.h"
#include "egl.h"
#include "mirror.h"
#include "event.h"

typedef struct ctx {
    ctx_opt_t opt;
    ctx_wl_t wl;
    ctx_egl_t egl;
    ctx_mirror_t mirror;
    ctx_event_t event;
} ctx_t;

noreturn void exit_fail(ctx_t * ctx);
void cleanup(ctx_t * ctx);

#endif
