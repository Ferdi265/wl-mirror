#ifndef WL_MIRROR_STREAM_H_
#define WL_MIRROR_STREAM_H_

#include <stddef.h>
#include <stdbool.h>

#include <wlm/event.h>

struct ctx;

typedef struct ctx_stream {
    // holds stream input lines prior to being parsed as options
    // holds partial input lines between calls to stream::on_stream_data()
    //
    // ownership:
    // - resized in stream::input_reserve()
    // - written in stream::on_stream_data()
    // - written in stream::on_line() (passed line partially overwritten)
    char * input;
    size_t input_len;
    size_t input_cap;

    // holds parsed argv array that will be parsed as options
    // empty between calls to stream::on_line()
    //
    // ownership:
    // - resized in stream::args_push()
    // - written in stream::on_line()
    char ** args;
    size_t args_len;
    size_t args_cap;

    event_handler_t event_handler;
    bool initialized;
} ctx_stream_t;

void wlm_stream_init(struct ctx * ctx);
void wlm_stream_cleanup(struct ctx * ctx);

#endif
