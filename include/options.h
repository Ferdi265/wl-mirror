#ifndef WL_MIRROR_OPTIONS_H_
#define WL_MIRROR_OPTIONS_H_

#include <stdint.h>
#include <stdbool.h>
#include "transform.h"

struct ctx;
struct output_list_node;

typedef enum {
    SCALE_FIT,
    SCALE_COVER,
    SCALE_EXACT
} scale_t;

typedef enum {
    SCALE_FILTER_LINEAR,
    SCALE_FILTER_NEAREST,
} scale_filter_t;

typedef enum {
    BACKEND_AUTO,
    BACKEND_DMABUF,
    BACKEND_SCREENCOPY,
#ifdef WITH_XDG_PORTAL_BACKEND
    BACKEND_XDG_PORTAL,
#endif
} backend_t;

typedef struct ctx_opt {
    bool verbose;
    bool stream;
    bool show_cursor;
    bool invert_colors;
    bool freeze;
    bool has_region;
    bool fullscreen;
    scale_t scaling;
    scale_filter_t scaling_filter;
    backend_t backend;
    transform_t transform;
    region_t region;
    char * output;
    char * fullscreen_output;
} ctx_opt_t;

void init_opt(struct ctx * ctx);
void cleanup_opt(struct ctx * ctx);

bool parse_scaling_opt(scale_t * scaling, scale_filter_t * scaling_filter, const char * scaling_arg);
bool parse_transform_opt(transform_t * transform, const char * transform_arg);
bool parse_region_opt(region_t * region, char ** output, const char * region_arg);
bool find_output_opt(struct ctx * ctx, struct output_list_node ** output_handle, region_t * region_handle);

void usage_opt(struct ctx * ctx);
void version_opt(struct ctx * ctx);

void parse_opt(struct ctx * ctx, int argc, char ** argv);

#endif
