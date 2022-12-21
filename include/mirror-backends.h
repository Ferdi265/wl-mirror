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

void init_mirror_dmabuf(struct ctx * ctx);
void init_mirror_screencopy(struct ctx * ctx);
void init_mirror_xdg_portal(struct ctx * ctx);

#endif
