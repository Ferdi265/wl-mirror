#ifndef WL_MIRROR_WAYLAND_REGISTRY_H_
#define WL_MIRROR_WAYLAND_REGISTRY_H_

#include <stdbool.h>
#include <stddef.h>
#include <wayland-client-core.h>

typedef struct ctx ctx_t;

typedef struct {
    const struct wl_interface * interface;
    size_t version_min;
    size_t version_max;
    bool required;
} wlm_wayland_registry_bind_spec_t;

typedef struct {
    wlm_wayland_registry_bind_spec_t spec;
    void (* on_add)(ctx_t *, struct wl_proxy * proxy);
    void (* on_remove)(ctx_t *, struct wl_proxy * proxy);
} wlm_wayland_registry_bind_multiple_t;

#define WLM_WAYLAND_REGISTRY_BIND_MULTIPLE(proxy_interface, min_version, max_version, is_required, on_add_fn, on_remove_fn) \
    (wlm_wayland_registry_bind_multiple_t){ \
        .spec = (wlm_wayland_registry_bind_spec_t) { \
            .interface = &proxy_interface, \
            .version_min = min_version, \
            .version_max = max_version, \
            .required = is_required, \
        }, \
        .on_add = (void(*)(struct ctx *, struct wl_proxy *))on_add_fn, \
        .on_remove = (void(*)(struct ctx *, struct wl_proxy *))on_remove_fn \
    }

#define WLM_WAYLAND_REGISTRY_BIND_MULTIPLE_END \
    (wlm_wayland_registry_bind_multiple_t){ \
        .spec = (wlm_wayland_registry_bind_spec_t) { \
            .interface = NULL, \
        }, \
    }

typedef struct {
    wlm_wayland_registry_bind_spec_t spec;
    size_t proxy_offset;
} wlm_wayland_registry_bind_singleton_t;

#define WLM_WAYLAND_REGISTRY_BIND_SINGLETON(proxy_interface, min_version, max_version, is_required, ctx_type, proxy_member) \
    (wlm_wayland_registry_bind_singleton_t){ \
        .spec = (wlm_wayland_registry_bind_spec_t) { \
            .interface = &proxy_interface, \
            .version_min = min_version, \
            .version_max = max_version, \
            .required = is_required, \
        }, \
        .proxy_offset = offsetof(ctx_type, proxy_member), \
    }

#define WLM_WAYLAND_REGISTRY_BIND_SINGLETON_END \
    (wlm_wayland_registry_bind_singleton_t){ \
        .spec = (wlm_wayland_registry_bind_spec_t) { \
            .interface = NULL, \
        }, \
    }

typedef struct {
    struct wl_list link;
    struct wl_proxy * proxy;
    const struct wl_interface * interface;
    uint32_t id;
    void (* on_remove)(ctx_t *, struct wl_proxy * proxy);
} wlm_wayland_registry_bound_global_t;

extern const wlm_wayland_registry_bind_multiple_t wlm_wayland_registry_bind_multiple[];
extern const wlm_wayland_registry_bind_singleton_t wlm_wayland_registry_bind_singleton[];

typedef struct {
    // registry handle
    struct wl_registry * handle;

    // bound registry globals
    struct wl_list bound_globals;

    // initial global sync flags
    struct wl_callback * sync_callback;
    bool initial_sync_had_errors;

    bool init_called;
    bool init_done;
} ctx_wl_registry_t;

void wlm_wayland_registry_zero(ctx_t *);
void wlm_wayland_registry_init(ctx_t *);
void wlm_wayland_registry_cleanup(ctx_t *);

bool wlm_wayland_registry_is_own_proxy(struct wl_proxy * proxy);

bool wlm_wayland_registry_is_init_called(ctx_t *);
bool wlm_wayland_registry_is_init_done(ctx_t *);

#endif
