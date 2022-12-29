#ifndef WL_MIRROR_STREAM_H_
#define WL_MIRROR_STREAM_H_

#include <stddef.h>
#include <stdbool.h>

#include "event.h"

struct ctx;

typedef struct ctx_stream {
    char * line;
    size_t line_len;
    size_t line_cap;

    char ** args;
    size_t args_len;
    size_t args_cap;

    event_handler_t event_handler;
    bool initialized;
} ctx_stream_t;

void init_stream(struct ctx * ctx);
void cleanup_stream(struct ctx * ctx);

#endif
