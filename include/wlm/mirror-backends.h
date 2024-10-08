#ifndef WL_MIRROR_MIRROR_BACKENDS_H_
#define WL_MIRROR_MIRROR_BACKENDS_H_

#include <stddef.h>

struct ctx;

#define MIRROR_BACKEND_FATAL_FAILCOUNT 10

typedef struct mirror_backend {
    void (*do_capture)(struct ctx * ctx);
    void (*do_cleanup)(struct ctx * ctx);
    size_t fail_count;
} mirror_backend_t;

void wlm_mirror_dmabuf_init(struct ctx * ctx);
void wlm_mirror_screencopy_init(struct ctx * ctx);

#endif
