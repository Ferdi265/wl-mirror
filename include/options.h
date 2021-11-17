#ifndef WL_MIRROR_OPTIONS_H_
#define WL_MIRROR_OPTIONS_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SCALE_LINEAR,
    SCALE_NEAREST,
    SCALE_EXACT
} scale_t;

typedef struct ctx_opt {
    bool verbose;
    bool show_cursor;
    scale_t scaling;
    const char * output;
} ctx_opt_t;

#endif
