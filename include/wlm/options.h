#ifndef WL_MIRROR_OPTIONS_H_
#define WL_MIRROR_OPTIONS_H_

#include <stdint.h>
#include <stdbool.h>
#include <wlm/transform.h>

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
    BACKEND_EXPORT_DMABUF,
    BACKEND_SCREENCOPY_AUTO,
    BACKEND_SCREENCOPY_SHM,
    BACKEND_SCREENCOPY_DMABUF,
    BACKEND_EXTCOPY_AUTO,
    BACKEND_EXTCOPY_SHM,
    BACKEND_EXTCOPY_DMABUF,
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
    char * window_title;
} ctx_opt_t;

void wlm_opt_init(struct ctx * ctx);
void wlm_cleanup_opt(struct ctx * ctx);

bool wlm_opt_parse_scaling(scale_t * scaling, scale_filter_t * scaling_filter, const char * scaling_arg);
bool wlm_opt_parse_backend(backend_t * backend, const char * backend_arg);
bool wlm_opt_parse_transform(transform_t * transform, const char * transform_arg);
bool wlm_opt_parse_region(region_t * region, char ** output, const char * region_arg);
bool wlm_opt_find_output(struct ctx * ctx, struct output_list_node ** output_handle, region_t * region_handle);

void wlm_opt_usage(struct ctx * ctx);
void wlm_opt_version(struct ctx * ctx);

void wlm_opt_parse(struct ctx * ctx, int argc, char ** argv);

#endif
