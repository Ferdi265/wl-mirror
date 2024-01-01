#ifndef WL_MIRROR_WAYLAND_H_
#define WL_MIRROR_WAYLAND_H_

#include <stdint.h>
#include <stdbool.h>
#include "event.h"
#include "viewporter.h"
#include "fractional-scale-v1.h"
#include "xdg-shell.h"
#include "xdg-output-unstable-v1.h"
#include "wlr-export-dmabuf-unstable-v1.h"
#include "wlr-screencopy-unstable-v1.h"

struct ctx;

typedef struct output_list_node {
    struct output_list_node * next;
    struct ctx * ctx;
    char * name;
    struct wl_output * output;
    struct zxdg_output_v1 * xdg_output;
    uint32_t output_id;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    int32_t scale;
    enum wl_output_transform transform;
} output_list_node_t;

typedef struct ctx_wl {
    struct wl_display * display;
    struct wl_registry * registry;

    // registry objects
    struct wl_compositor * compositor;
    struct wl_seat * seat;
    struct wp_viewporter * viewporter;
    struct wp_fractional_scale_manager_v1 * fractional_scale_manager;
    struct xdg_wm_base * wm_base;
    struct zxdg_output_manager_v1 * output_manager;
    // registry ids
    uint32_t compositor_id;
    uint32_t seat_id;
    uint32_t viewporter_id;
    uint32_t fractional_scale_manager_id;
    uint32_t wm_base_id;
    uint32_t output_manager_id;

    // dmabuf backend objects
    struct zwlr_export_dmabuf_manager_v1 * dmabuf_manager;
    uint32_t dmabuf_manager_id;

    // screencopy backend objects
    struct wl_shm * shm;
    struct zwlr_screencopy_manager_v1 * screencopy_manager;
    uint32_t shm_id;
    uint32_t screencopy_manager_id;

    // output list
    output_list_node_t * outputs;

    // surface objects
    struct wl_surface * surface;
    struct wp_viewport * viewport;
    struct wp_fractional_scale_v1 * fractional_scale;
    struct xdg_surface * xdg_surface;
    struct xdg_toplevel * xdg_toplevel;

    // buffer size
    output_list_node_t * current_output;
    uint32_t width;
    uint32_t height;
    int32_t buffer_scale;
    double scale;

    // event handler
    event_handler_t event_handler;

    // state flags
    uint32_t last_surface_serial;
    bool xdg_surface_configured;
    bool xdg_toplevel_configured;
    bool configured;
    bool closing;
    bool initialized;
} ctx_wl_t;

void init_wl(struct ctx * ctx);
void set_window_title(struct ctx * ctx, const char * title);
void set_window_fullscreen(struct ctx * ctx);
void unset_window_fullscreen(struct ctx * ctx);
void update_window_scale(struct ctx * ctx, double scale, bool is_fractional);
void cleanup_wl(struct ctx * ctx);

#endif
