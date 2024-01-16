#include <stdlib.h>
#include <string.h>
#include <wlm/context.h>

// --- private utility functions ---

static const char * proxy_tag = "wl-mirror";

static struct wl_proxy * try_bind(
    ctx_t * ctx, uint32_t id, uint32_t version,
    const wlm_wayland_registry_bind_spec_t * spec,
    void (* on_remove)(struct ctx *, struct wl_proxy *)
) {
    if (version < spec->version_min) {
        if (spec->required) {
            log_error("wayland::registry::try_bind(): interface %s only available in version %d, but %zd required\n",
                spec->interface->name, version, spec->version_min
            );

            if (ctx->wl.registry.initial_sync_complete) {
                wlm_exit_fail(ctx);
            } else {
                ctx->wl.registry.initial_sync_had_errors = true;
            }
        }

        return NULL;
    } else if (version > spec->version_max) {
        version = spec->version_max;
    }

    struct wl_proxy * proxy = wl_registry_bind(ctx->wl.registry.handle, id, spec->interface, version);
    wl_proxy_set_tag(proxy, &proxy_tag);

    wlm_wayland_registry_bound_global_t * bound_global = malloc(sizeof *bound_global);
    bound_global->proxy = proxy;
    bound_global->interface = spec->interface;
    bound_global->id = id;
    bound_global->on_remove = on_remove;
    wl_list_insert(&ctx->wl.registry.bound_globals, &bound_global->link);

    return proxy;
}

static void try_bind_multiple(ctx_t * ctx, uint32_t id, uint32_t version, const wlm_wayland_registry_bind_multiple_t * bind) {
    struct wl_proxy * proxy = try_bind(ctx, id, version, &bind->spec, bind->on_remove);
    if (proxy == NULL) {
        return;
    }

    bind->on_add(ctx, proxy);
}

static void try_bind_singleton(ctx_t * ctx, uint32_t id, uint32_t version, const wlm_wayland_registry_bind_singleton_t * bind) {
    struct wl_proxy ** proxy_ptr = (struct wl_proxy **)((char *)ctx + bind->proxy_offset);
    if (*proxy_ptr != NULL) {
        log_error("wayland::registry::try_bind_singleton(): duplicate singleton %s\n", bind->spec.interface->name);

        if (ctx->wl.registry.initial_sync_complete) {
            wlm_exit_fail(ctx);
        } else {
            ctx->wl.registry.initial_sync_had_errors = true;
        }

        return;
    }

    struct wl_proxy * proxy = try_bind(ctx, id, version, &bind->spec, NULL);
    if (proxy == NULL) {
        return;
    }

    *proxy_ptr = proxy;
}

// --- registry event handlers ---

static void on_registry_add(
    void * data, struct wl_registry * registry,
    uint32_t id, const char * interface, uint32_t version
) {
    if (registry == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    log_debug(ctx, "wayland::registry::on_registry_add(): %s (version = %d, id = %d)\n", interface, version, id);

    const wlm_wayland_registry_bind_multiple_t * bind_multiple = wlm_wayland_registry_bind_multiple;
    for (; bind_multiple->spec.interface != NULL; bind_multiple++) {
        if (strcmp(interface, bind_multiple->spec.interface->name) != 0) continue;

        try_bind_multiple(ctx, id, version, bind_multiple);
        return;
    }

    const wlm_wayland_registry_bind_singleton_t * bind_singleton = wlm_wayland_registry_bind_singleton;
    for (; bind_singleton->spec.interface != NULL; bind_singleton++) {
        if (strcmp(interface, bind_singleton->spec.interface->name) != 0) continue;

        try_bind_singleton(ctx, id, version, bind_singleton);
        return;
    }
}

static void on_registry_remove(
    void * data, struct wl_registry * registry,
    uint32_t id
) {
    if (registry == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    log_debug(ctx, "wayland::registry::on_registry_remove(): id = %d\n", id);

    wlm_wayland_registry_bound_global_t *cur, *next;
    wl_list_for_each_safe(cur, next, &ctx->wl.registry.bound_globals, link) {
        if (cur->id != id) continue;
        if (cur->on_remove == NULL) {
            log_error("wayland::registry::on_registry_remove(): singleton interface %s removed\n", cur->interface->name);
            wlm_exit_fail(ctx);
        }

        cur->on_remove(ctx, cur->proxy);

        wl_list_remove(&cur->link);
        wl_proxy_destroy(cur->proxy);
        free(cur);
    }
}

static const struct wl_registry_listener registry_listener = {
    .global = on_registry_add,
    .global_remove = on_registry_remove,
};

// --- initial sync event handler ---

static void on_sync_callback_done(
    void * data, struct wl_callback * callback,
    uint32_t callback_data
) {
    if (callback == NULL) return;

    ctx_t * ctx = (ctx_t *)data;
    log_debug(ctx, "wayland::registry::on_sync_callback_done(): all globals received\n");

    wl_callback_destroy(ctx->wl.registry.sync_callback);
    ctx->wl.registry.sync_callback = NULL;

    // check if all required singletons bound
    const wlm_wayland_registry_bind_singleton_t * bind_singleton = wlm_wayland_registry_bind_singleton;
    for (; bind_singleton->spec.interface != NULL; bind_singleton++) {
        struct wl_proxy ** proxy_ptr = (struct wl_proxy **)((char *)ctx + bind_singleton->proxy_offset);

        if (*proxy_ptr != NULL)
            log_debug(ctx, "wayland::registry::on_sync_callback_done(): bound %s (version = %d)\n", bind_singleton->spec.interface->name, wl_proxy_get_version(*proxy_ptr));
        if (*proxy_ptr == NULL && bind_singleton->spec.required) {
            log_error("wayland::registry::on_sync_callback_done(): required singleton interface %s missing\n", bind_singleton->spec.interface->name);
            ctx->wl.registry.initial_sync_had_errors = true;
        }
    }

    ctx->wl.registry.initial_sync_complete = true;
    if (ctx->wl.registry.initial_sync_had_errors) {
        wlm_exit_fail(ctx);
    }

    wlm_event_emit_registry_init_done(ctx);

    (void)callback_data;
}

static const struct wl_callback_listener sync_callback_listener = {
    .done = on_sync_callback_done,
};

// --- initialization and cleanup ---

void wlm_wayland_registry_zero(ctx_t * ctx) {
    // registry handle
    ctx->wl.registry.handle = NULL;

    // bound registry globals
    wl_list_init(&ctx->wl.registry.bound_globals);

    // reset initial sync flags
    ctx->wl.registry.sync_callback = NULL;
    ctx->wl.registry.initial_sync_had_errors = false;
    ctx->wl.registry.initial_sync_complete = false;

    // clear all bound singleton proxies
    const wlm_wayland_registry_bind_singleton_t * bind_singleton = wlm_wayland_registry_bind_singleton;
    for (; bind_singleton->spec.interface != NULL; bind_singleton++) {
        *(struct wl_proxy **)((char *)ctx + bind_singleton->proxy_offset) = NULL;
    }
}

void wlm_wayland_registry_init(ctx_t * ctx) {
    // get registry handle
    ctx->wl.registry.handle = wl_display_get_registry(ctx->wl.core.display);
    if (ctx->wl.registry.handle == NULL) {
        log_error("wayland::registry::init(): failed to get registry handle\n");
        wlm_exit_fail(ctx);
    }

    wl_registry_add_listener(ctx->wl.registry.handle, &registry_listener, (void *)ctx);

    // perform initial sync
    ctx->wl.registry.sync_callback = wl_display_sync(ctx->wl.core.display);

    wl_callback_add_listener(ctx->wl.registry.sync_callback, &sync_callback_listener, (void *)ctx);
}

void wlm_wayland_registry_cleanup(ctx_t * ctx) {
    // destroy sync callback
    if (ctx->wl.registry.sync_callback != NULL) wl_callback_destroy(ctx->wl.registry.sync_callback);

    // destroy bound registry globals
    wlm_wayland_registry_bound_global_t *cur, *next;
    wl_list_for_each_safe(cur, next, &ctx->wl.registry.bound_globals, link) {
        wl_list_remove(&cur->link);
        wl_proxy_destroy(cur->proxy);
        free(cur);
    }

    // destroy registry handle
    if (ctx->wl.registry.handle != NULL) wl_registry_destroy(ctx->wl.registry.handle);

    wlm_wayland_registry_zero(ctx);
}

// --- public functions ---

bool wlm_wayland_registry_is_initial_sync_complete(ctx_t * ctx) {
    return ctx->wl.registry.initial_sync_complete;
}

bool wlm_wayland_registry_is_own_proxy(struct wl_proxy * proxy) {
    return wl_proxy_get_tag(proxy) == &proxy_tag;
}
