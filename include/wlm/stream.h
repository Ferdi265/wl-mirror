#ifndef WL_MIRROR_STREAM_H_
#define WL_MIRROR_STREAM_H_

#include <stddef.h>
#include <stdbool.h>

#include <wlm/event.h>

struct ctx;

typedef struct ctx_stream {
    // lifetime: written in stream::on_stream_data.
    // All full lines are overwritten at the end of the function.
    // Trailing bytes which are not newline-terminated are treated as part of a line
    // which was not read fully.
    // These bytes are moved to the beginning of the buffer
    // in order to continue reading the line once more data becomes available.
    char * line;
    size_t line_len;
    size_t line_cap;

    // lifetime: written in stream::on_line, overwritten at the end of the function
    char ** args;
    size_t args_len;
    size_t args_cap;

    event_handler_t event_handler;
    bool initialized;
} ctx_stream_t;

void wlm_stream_init(struct ctx * ctx);
void wlm_stream_cleanup(struct ctx * ctx);

#endif
