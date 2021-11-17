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

bool parse_scaling_option(scale_t * scaling, const char * scaling_str);
bool parse_transform_option(transform_t * transform, const char * transform_str);

#endif
