#ifndef WL_MIRROR_WAYLAND_REGISTRY_H_
#define WL_MIRROR_WAYLAND_REGISTRY_H_

#include <stdbool.h>
#include <stddef.h>
#include <wayland-client-core.h>

typedef struct ctx ctx_t;

typedef struct {
    const struct wl_interface * interface;
    size_t version;
    bool required;
    void (* on_add)(ctx_t *, struct wl_proxy * proxy);
    void (* on_remove)(ctx_t *, struct wl_proxy * proxy);
} wayland_registry_bind_multiple_t;

#define WAYLAND_REGISTRY_BIND_MULTIPLE(proxy_interface, min_version, is_required, on_add_fn, on_remove_fn) \
    (wayland_registry_bind_multiple_t){ \
        .interface = &proxy_interface, \
        .version = min_version, \
        .required = is_required, \
        .on_add = (void(*)(struct ctx *, struct wl_proxy *))on_add_fn, \
        .on_remove = (void(*)(struct ctx *, struct wl_proxy *))on_remove_fn \
    }

#define WAYLAND_REGISTRY_BIND_MULTIPLE_END \
    (wayland_registry_bind_multiple_t){ \
        .interface = NULL \
    }

typedef struct {
    const struct wl_interface * interface;
    size_t version;
    size_t proxy_offset;
    bool required;
} wayland_registry_bind_singleton_t;

#define WAYLAND_REGISTRY_BIND_SINGLETON(proxy_interface, min_version, is_required, ctx_type, proxy_member) \
    (wayland_registry_bind_singleton_t){ \
        .interface = &proxy_interface, \
        .version = min_version, \
        .proxy_offset = offsetof(ctx_type, proxy_member), \
        .required = is_required \
    }

#define WAYLAND_REGISTRY_BIND_SINGLETON_END \
    (wayland_registry_bind_singleton_t){ \
        .interface = NULL \
    }

typedef struct {
    struct wl_list link;
    struct wl_proxy * proxy;
    const struct wl_interface * interface;
    uint32_t id;
    void (* on_remove)(ctx_t *, struct wl_proxy * proxy);
} wayland_registry_bound_global_t;

extern const wayland_registry_bind_multiple_t wayland_registry_bind_multiple[];
extern const wayland_registry_bind_singleton_t wayland_registry_bind_singleton[];

typedef struct {
    // registry handle
    struct wl_registry * handle;

    // bound registry globals
    struct wl_list bound_globals;

    // initial global sync flags
    struct wl_callback * sync_callback;
    bool initial_sync_had_errors;
    bool initial_sync_complete;
} ctx_wl_registry_t;

void wayland_registry_zero(ctx_t *);
void wayland_registry_init(ctx_t *);
void wayland_registry_cleanup(ctx_t *);

bool wayland_registry_is_initial_sync_complete(ctx_t *);
bool wayland_registry_is_own_proxy(struct wl_proxy * proxy);

#endif
