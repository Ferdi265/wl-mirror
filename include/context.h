#ifndef WL_MIRROR_CONTEXT_H_
#define WL_MIRROR_CONTEXT_H_

struct wl_display;
struct wl_registry;
struct xdg_wm_base;

typedef struct {
    struct wl_display * display;
    struct wl_registry * registry;

    // registry objects
    struct wl_compositor * compositor;
    struct xdg_wm_base * xdg_wm_base;
} wl_ctx_t;

typedef struct {

} egl_ctx_t;

typedef struct {

} mirror_ctx_t;

typedef struct {
    wl_ctx_t * wl;
    egl_ctx_t * egl;
    mirror_ctx_t * mirror;
} ctx_t;

#endif
