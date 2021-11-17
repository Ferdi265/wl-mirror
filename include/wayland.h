#ifndef WL_MIRROR_WAYLAND_H_
#define WL_MIRROR_WAYLAND_H_

#include <stdint.h>
#include <stdbool.h>
#include "xdg-shell.h"
#include "xdg-output-unstable-v1.h"
#include "wlr-export-dmabuf-unstable-v1.h"

struct ctx;

typedef struct output_list_node {
    struct output_list_node * next;
    struct ctx * ctx;
    char * name;
    struct wl_output * output;
    struct zxdg_output_v1 * xdg_output;
    uint32_t output_id;
    int32_t scale;
} output_list_node_t;

typedef struct ctx_wl {
    struct wl_display * display;
    struct wl_registry * registry;

    // registry objects
    struct wl_compositor * compositor;
    struct xdg_wm_base * wm_base;
    struct zxdg_output_manager_v1 * output_manager;
    struct zwlr_export_dmabuf_manager_v1 * dmabuf_manager;
    // registry ids
    uint32_t compositor_id;
    uint32_t wm_base_id;
    uint32_t output_manager_id;
    uint32_t dmabuf_manager_id;

    // output list
    output_list_node_t * outputs;

    // surface objects
    struct wl_surface * surface;
    struct xdg_surface * xdg_surface;
    struct xdg_toplevel * xdg_toplevel;

    // buffer size
    output_list_node_t * current_output;
    uint32_t width;
    uint32_t height;
    int32_t scale;

    // state flags
    uint32_t last_surface_serial;
    bool xdg_surface_configured;
    bool xdg_toplevel_configured;
    bool configured;
    bool closing;
} ctx_wl_t;

void init_wl(struct ctx * ctx);

void cleanup_wl(struct ctx * ctx);

#endif
