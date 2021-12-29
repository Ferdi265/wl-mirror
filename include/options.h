#ifndef WL_MIRROR_OPTIONS_H_
#define WL_MIRROR_OPTIONS_H_

#include <stdint.h>
#include <stdbool.h>
#include "transform.h"

struct ctx;

typedef enum {
    SCALE_LINEAR,
    SCALE_NEAREST,
    SCALE_EXACT
} scale_t;

typedef struct ctx_opt {
    bool verbose;
    bool stream;
    bool show_cursor;
    bool invert_colors;
    bool has_region;
    scale_t scaling;
    transform_t transform;
    region_t region;
    char * output;
} ctx_opt_t;

void init_opt(struct ctx * ctx);
void cleanup_opt(struct ctx * ctx);

bool parse_scaling_opt(scale_t * scaling, const char * scaling_arg);
bool parse_transform_opt(transform_t * transform, const char * transform_arg);
bool parse_region_opt(region_t * region, char ** output, const char * region_arg);

void usage_opt(struct ctx * ctx);

void parse_opt(struct ctx * ctx, int argc, char ** argv);

#endif
