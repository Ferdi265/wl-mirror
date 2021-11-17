#ifndef WL_MIRROR_OPTIONS_H_
#define WL_MIRROR_OPTIONS_H_

#include <stdint.h>
#include <stdbool.h>
#include "transform.h"

typedef enum {
    SCALE_LINEAR,
    SCALE_NEAREST,
    SCALE_EXACT
} scale_t;

typedef struct ctx_opt {
    bool verbose;
    bool show_cursor;
    scale_t scaling;
    transform_t transform;
    const char * output;
} ctx_opt_t;

#endif
