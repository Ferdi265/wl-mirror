#ifndef WL_MIRROR_WAYLAND_H_
#define WL_MIRROR_WAYLAND_H_

#include <stdint.h>
#include <stdbool.h>
#include <wlm/event.h>
#include <wlm/proto/viewporter.h>
#include <wlm/proto/fractional-scale-v1.h>
#include <wlm/proto/xdg-shell.h>
#include <wlm/proto/xdg-output-unstable-v1.h>
#include <wlm/proto/wlr-export-dmabuf-unstable-v1.h>
#include <wlm/proto/wlr-screencopy-unstable-v1.h>
#include <wlm/proto/ext-image-copy-capture-v1.h>
#include <wlm/proto/ext-image-capture-source-v1.h>
#include <wlm/proto/linux-dmabuf-unstable-v1.h>
#include <wlm/wayland/shm.h>
#include <wlm/wayland/dmabuf.h>

#ifdef WITH_LIBDECOR
#include <libdecor.h>
#endif

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

typedef struct seat_list_node {
    struct seat_list_node * next;
    struct ctx * ctx;
    struct wl_seat * seat;
    uint32_t seat_id;
} seat_list_node_t;

typedef struct ctx_wl {
    ctx_wl_shm_t shmbuf;
    ctx_wl_dmabuf_t dmabuf;

    struct wl_display * display;
    struct wl_registry * registry;

    // registry objects
    struct wl_compositor * compositor;
    struct wp_viewporter * viewporter;
    struct wp_fractional_scale_manager_v1 * fractional_scale_manager;
    struct xdg_wm_base * wm_base;
    struct zxdg_output_manager_v1 * output_manager;
    // registry ids
    uint32_t compositor_id;
    uint32_t viewporter_id;
    uint32_t fractional_scale_manager_id;
    uint32_t wm_base_id;
    uint32_t output_manager_id;

    // shm and dmabuf objects
    struct wl_shm * shm;
    uint32_t shm_id;
    struct zwp_linux_dmabuf_v1 * linux_dmabuf;
    uint32_t linux_dmabuf_id;

    // dmabuf backend objects
    struct zwlr_export_dmabuf_manager_v1 * dmabuf_manager;
    uint32_t dmabuf_manager_id;

    // screencopy backend objects
    struct zwlr_screencopy_manager_v1 * screencopy_manager;
    uint32_t screencopy_manager_id;

    // extcopy backend objects
    struct ext_image_copy_capture_manager_v1 * copy_capture_manager;
    struct ext_output_image_capture_source_manager_v1 * output_capture_source_manager;
    struct ext_foreign_toplevel_image_capture_source_manager_v1 * toplevel_capture_source_manager;
    uint32_t copy_capture_manager_id;
    uint32_t output_capture_source_manager_id;
    uint32_t toplevel_capture_source_manager_id;

    // output list
    output_list_node_t * outputs;
    seat_list_node_t * seats;

    // surface objects
    struct wl_surface * surface;
    struct wp_viewport * viewport;
    struct wp_fractional_scale_v1 * fractional_scale;
#ifdef WITH_LIBDECOR
    struct libdecor * libdecor_context;
    struct libdecor_frame * libdecor_frame;
#else
    struct xdg_surface * xdg_surface;
    struct xdg_toplevel * xdg_toplevel;
#endif

    // buffer size
    output_list_node_t * current_output;
    uint32_t width;
    uint32_t height;
    double scale;

    // event handler
    event_handler_t event_handler;

    // state flags
    uint32_t last_surface_serial;
#ifndef WITH_LIBDECOR
    bool xdg_surface_configured;
    bool xdg_toplevel_configured;
#endif
    bool configured;
    bool closing;
    bool initialized;
} ctx_wl_t;

void wlm_wayland_init(struct ctx * ctx);
void wlm_wayland_configure_window(struct ctx * ctx);
void wlm_wayland_window_close(struct ctx * ctx);
void wlm_wayland_window_set_title(struct ctx * ctx, const char * title);
void wlm_wayland_window_set_fullscreen(struct ctx * ctx);
void wlm_wayland_window_unset_fullscreen(struct ctx * ctx);
void wlm_wayland_window_update_scale(struct ctx * ctx, double scale, bool is_fractional);
void wlm_wayland_cleanup(struct ctx * ctx);

#endif
