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
#include <wlm/proto/ext-foreign-toplevel-list-v1.h>
#include <wlm/proto/linux-dmabuf-unstable-v1.h>
#include <wlm/wayland/shm.h>
#include <wlm/wayland/dmabuf.h>
#include <wlm/wayland/core.h>
#include <wlm/wayland/registry.h>
#include <wlm/wayland/protocols.h>
#include <wlm/wayland/output.h>
#include <wlm/wayland/seat.h>

#ifdef WITH_LIBDECOR
#include <libdecor.h>
#endif

struct ctx;

typedef struct wlm_wayland_output_entry {
    struct wlm_wayland_output_entry * next;
    struct ctx * ctx;
    char * name;
    char * make;
    char * model;
    char * description;
    struct wl_output * output;
    struct zxdg_output_v1 * xdg_output;
    uint32_t output_id;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    int32_t scale;
    enum wl_output_transform transform;
} wlm_wayland_output_entry_t;

typedef struct wlm_wayland_seat_entry {
    struct wlm_wayland_seat_entry * next;
    struct ctx * ctx;
    struct wl_seat * seat;
    uint32_t seat_id;
} wlm_wayland_seat_entry_t;

typedef struct ctx_wl {
    ctx_wl_core_t core;
    ctx_wl_registry_t registry;
    ctx_wl_protocols_t protocols;
    ctx_wl_output_t output;
    ctx_wl_seat_t seat;

    ctx_wl_shm_t shmbuf;
    ctx_wl_dmabuf_t dmabuf;

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
    wlm_wayland_output_entry_t * current_output;
    uint32_t width;
    uint32_t height;
    double scale;

    // state flags
    uint32_t last_surface_serial;
#ifndef WITH_LIBDECOR
    bool xdg_surface_configured;
    bool xdg_toplevel_configured;
#endif
    bool configured;
    bool initialized;
} ctx_wl_t;

void wlm_wayland_init(struct ctx * ctx);
void wlm_wayland_configure_window(struct ctx * ctx);
void wlm_wayland_window_set_title(struct ctx * ctx, const char * title);
void wlm_wayland_window_set_fullscreen(struct ctx * ctx);
void wlm_wayland_window_unset_fullscreen(struct ctx * ctx);
void wlm_wayland_window_update_scale(struct ctx * ctx, double scale, bool is_fractional);
bool wlm_wayland_find_output(ctx_t * ctx, const char * output_name, wlm_wayland_output_entry_t ** output);
void wlm_wayland_cleanup(struct ctx * ctx);

#endif
