#ifndef WL_MIRROR_WLM_WAYLAND_OUTPUT_H_
#define WL_MIRROR_WLM_WAYLAND_OUTPUT_H_

#include <stdbool.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include "xdg-output-unstable-v1.h"

typedef struct ctx ctx_t;

#define WLM_PRINT_OUTPUT_TRANSFORM(transform) ( \
        transform == WL_OUTPUT_TRANSFORM_NORMAL ? "NORMAL" : \
        transform == WL_OUTPUT_TRANSFORM_90 ? "90" : \
        transform == WL_OUTPUT_TRANSFORM_180 ? "180" : \
        transform == WL_OUTPUT_TRANSFORM_270 ? "270" : \
        transform == WL_OUTPUT_TRANSFORM_FLIPPED_90 ? "FLIPPED_90" : \
        transform == WL_OUTPUT_TRANSFORM_FLIPPED_180 ? "FLIPPED_180" : \
        transform == WL_OUTPUT_TRANSFORM_FLIPPED_270 ? "FLIPPED_270" : \
        "???" \
    )

typedef enum {
    WLM_WAYLAND_OUTPUT_NAME_CHANGED         = 1 << 0,
    WLM_WAYLAND_OUTPUT_POSITION_CHANGED     = 1 << 1,
    WLM_WAYLAND_OUTPUT_SIZE_CHANGED         = 1 << 2,
    WLM_WAYLAND_OUTPUT_SCALE_CHANGED        = 1 << 3,
    WLM_WAYLAND_OUTPUT_TRANSFORM_CHANGED    = 1 << 4,
    WLM_WAYLAND_OUTPUT_XDG_CHANGED          = 1 << 5,

    WLM_WAYLAND_OUTPUT_UNCHANGED            = 0,
    WLM_WAYLAND_OUTPUT_CHANGED              = -1U
} wlm_wayland_output_entry_changed_t;

typedef enum {
    WLM_WAYLAND_OUTPUT_WL_DONE              = 1 << 0,
    WLM_WAYLAND_OUTPUT_XDG_PARTIAL          = 1 << 1,
    WLM_WAYLAND_OUTPUT_XDG_DONE             = 1 << 2,
    WLM_WAYLAND_OUTPUT_COMPLETE             = 1 << 3,

    WLM_WAYLAND_OUTPUT_INCOMPLETE           = 0,
    WLM_WAYLAND_OUTPUT_READY                =
        WLM_WAYLAND_OUTPUT_WL_DONE |
        WLM_WAYLAND_OUTPUT_XDG_PARTIAL |
        WLM_WAYLAND_OUTPUT_XDG_DONE
} wlm_wayland_output_flags_t;

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

    wlm_wayland_output_entry_changed_t changed;
    wlm_wayland_output_flags_t flags;
} wlm_wayland_output_entry_t;

typedef struct {
    struct wl_list output_list;

    size_t incomplete_outputs;
} ctx_wl_output_t;

void wlm_wayland_output_on_add(ctx_t *, struct wl_output * output);
void wlm_wayland_output_on_remove(ctx_t *, struct wl_output * output);
void wlm_wayland_output_on_registry_initial_sync(ctx_t *);

void wlm_wayland_output_zero(ctx_t *);
void wlm_wayland_output_init(ctx_t *);
void wlm_wayland_output_cleanup(ctx_t *);

wlm_wayland_output_entry_t * wlm_wayland_output_find(ctx_t *, struct wl_output * output);
wlm_wayland_output_entry_t * wlm_wayland_output_find_by_name(ctx_t *, const char * name);

#endif
