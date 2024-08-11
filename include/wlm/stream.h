#ifndef WL_MIRROR_STREAM_H_
#define WL_MIRROR_STREAM_H_

#include <stddef.h>
#include <stdbool.h>

#include <wlm/event.h>

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

void wlm_stream_init(struct ctx * ctx);
void wlm_stream_cleanup(struct ctx * ctx);

#endif
