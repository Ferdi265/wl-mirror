#ifndef WL_MIRROR_WAYLAND_WINDOW_H_
#define WL_MIRROR_WAYLAND_WINDOW_H_

#include <libdecor.h>
#include <wlm/wayland/output.h>

typedef struct ctx ctx_t;

#define DEFAULT_WIDTH 100
#define DEFAULT_HEIGHT 100

typedef enum {
    WLM_WAYLAND_WINDOW_SIZE_CHANGED         = 1 << 0,
    WLM_WAYLAND_WINDOW_BUFFER_SIZE_CHANGED  = 1 << 1,
    WLM_WAYLAND_WINDOW_SCALE_CHANGED        = 1 << 2,
    WLM_WAYLAND_WINDOW_TRANSFORM_CHANGED    = 1 << 3,
    WLM_WAYLAND_WINDOW_OUTPUT_CHANGED       = 1 << 4,

    WLM_WAYLAND_WINDOW_UNCHANGED            = 0,
    WLM_WAYLAND_WINDOW_CHANGED              = -1U
} wlm_wayland_window_changed_t;

typedef enum {
    WLM_WAYLAND_WINDOW_TOPLEVEL_DONE        = 1 << 0,
    WLM_WAYLAND_WINDOW_OUTPUTS_DONE         = 1 << 1,
    WLM_WAYLAND_WINDOW_COMPLETE             = 1 << 3,

    WLM_WAYLAND_WINDOW_INCOMPLETE           = 0,
    WLM_WAYLAND_WINDOW_READY                =
        WLM_WAYLAND_WINDOW_TOPLEVEL_DONE |
        WLM_WAYLAND_WINDOW_OUTPUTS_DONE
} wlm_wayland_window_flags_t;

typedef struct {
    struct wl_surface * surface;
    struct wp_viewport * viewport;
    struct wp_fractional_scale_v1 * fractional_scale;
    struct libdecor_frame * libdecor_frame;

    wlm_wayland_output_entry_t * current_output;
    enum wl_output_transform transform;
    uint32_t width;
    uint32_t height;
    uint32_t buffer_width;
    uint32_t buffer_height;
    double scale;

    wlm_wayland_window_changed_t changed;
    wlm_wayland_window_flags_t flags;
} ctx_wl_window_t;

void wlm_wayland_window_on_output_init_done(ctx_t *);
void wlm_wayland_window_on_output_changed(ctx_t *, wlm_wayland_output_entry_t * entry);
void wlm_wayland_window_on_output_removed(ctx_t *, wlm_wayland_output_entry_t * entry);
void wlm_wayland_window_on_before_poll(ctx_t *);

void wlm_wayland_window_zero(ctx_t *);
void wlm_wayland_window_init(ctx_t *);
void wlm_wayland_window_cleanup(ctx_t *);

#endif
