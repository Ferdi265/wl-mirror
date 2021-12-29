#ifndef WL_MIRROR_EVENT_H_
#define WL_MIRROR_EVENT_H_

#include <stddef.h>

struct ctx;

typedef struct ctx_event {
    char * line;
    size_t line_len;
    size_t line_cap;

    char ** args;
    size_t args_len;
    size_t args_cap;
} ctx_event_t;

void init_event(struct ctx * ctx);
void cleanup_event(struct ctx * ctx);

void event_loop(struct ctx * ctx);

#endif
