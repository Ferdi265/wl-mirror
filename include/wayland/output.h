#ifndef WL_MIRROR_WAYLAND_OUTPUT_H_
#define WL_MIRROR_WAYLAND_OUTPUT_H_

#include <stdbool.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include "xdg-output-unstable-v1.h"

typedef struct ctx ctx_t;

typedef enum {
    WAYLAND_OUTPUT_NAME_CHANGED         = 1 << 0,
    WAYLAND_OUTPUT_POSITION_CHANGED     = 1 << 1,
    WAYLAND_OUTPUT_SIZE_CHANGED         = 1 << 2,
    WAYLAND_OUTPUT_SCALE_CHANGED        = 1 << 3,
    WAYLAND_OUTPUT_TRANSFORM_CHANGED    = 1 << 4,
    WAYLAND_OUTPUT_XDG_CHANGED          = 1 << 5,

    WAYLAND_OUTPUT_UNCHANGED            = 0,
    WAYLAND_OUTPUT_CHANGED              = -1U
} wayland_output_entry_changed_t;

typedef enum {
    WAYLAND_OUTPUT_INCOMPLETE           = 2,
    WAYLAND_OUTPUT_XDG_COMPLETE         = 1,
    WAYLAND_OUTPUT_COMPLETE             = 0
} wayland_output_completeness_t;

typedef struct {
    struct wl_list link;
    struct wl_output * output;
    struct zxdg_output_v1 * xdg_output;

    char * name;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    int32_t scale;
    enum wl_output_transform transform;

    wayland_output_entry_changed_t changed;
    wayland_output_completeness_t incomplete;
} wayland_output_entry_t;

typedef struct {
    struct wl_list output_list;

    size_t incomplete_outputs;
} ctx_wl_output_t;

void wayland_output_zero(ctx_t *);
void wayland_output_init(ctx_t *);
void wayland_output_cleanup(ctx_t *);

wayland_output_entry_t * wayland_output_find(ctx_t *, struct wl_output * output);
wayland_output_entry_t * wayland_output_find_by_name(ctx_t *, const char * name);

void wayland_output_on_add(ctx_t *, struct wl_output * output);
void wayland_output_on_remove(ctx_t *, struct wl_output * output);
void wayland_output_on_initial_sync(ctx_t *);

#endif
