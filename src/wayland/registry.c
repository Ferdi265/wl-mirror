#include <stdlib.h>
#include <string.h>
#include "context.h"
#include "wayland.h"

// --- private utility functions ---

static const char * proxy_tag = "wl-mirror";

static struct wl_proxy * try_bind(
    ctx_t * ctx, uint32_t id, uint32_t version,
    const struct wl_interface * interface, uint32_t min_version, bool required,
    void (* on_remove)(struct ctx *, struct wl_proxy *)
) {
    if (version < min_version) {
        if (required) {
            log_error("wayland::registry::try_bind(): interface %s only available in version %d, but %d required\n",
                interface->name, version, min_version
            );

            if (ctx->wl.registry.initial_sync_complete) {
                exit_fail(ctx);
            } else {
                ctx->wl.registry.initial_sync_had_errors = true;
            }
        }

        return NULL;
    }

    log_debug(ctx, "wayland::registry::try_bind(): binding %s (version = %d)\n", interface->name, min_version);
    struct wl_proxy * proxy = wl_registry_bind(ctx->wl.registry.handle, id, interface, min_version);
    wl_proxy_set_tag(proxy, &proxy_tag);

    wayland_registry_bound_global_t * bound_global = malloc(sizeof *bound_global);
    bound_global->proxy = proxy;
    bound_global->interface = interface;
    bound_global->id = id;
    bound_global->on_remove = on_remove;
    wl_list_insert(&ctx->wl.registry.bound_globals, &bound_global->link);

    return proxy;
}

static void try_bind_multiple(ctx_t * ctx, uint32_t id, uint32_t version, const wayland_registry_bind_multiple_t * spec) {
    struct wl_proxy * proxy = try_bind(ctx, id, version, spec->interface, spec->version, spec->required, spec->on_remove);
    if (proxy == NULL) {
        return;
    }

    spec->on_add(ctx, proxy);
}

static void try_bind_singleton(ctx_t * ctx, uint32_t id, uint32_t version, const wayland_registry_bind_singleton_t * spec) {
    struct wl_proxy ** proxy_ptr = (struct wl_proxy **)((char *)ctx + spec->proxy_offset);
    if (*proxy_ptr != NULL) {
        log_error("wayland::registry::try_bind_singleton(): duplicate singleton %s\n", spec->interface->name);

        if (ctx->wl.registry.initial_sync_complete) {
            exit_fail(ctx);
        } else {
            ctx->wl.registry.initial_sync_had_errors = true;
        }

        return;
    }

    struct wl_proxy * proxy = try_bind(ctx, id, version, spec->interface, spec->version, spec->required, NULL);
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

    const wayland_registry_bind_multiple_t * bind_multiple_spec = wayland_registry_bind_multiple;
    for (; bind_multiple_spec->interface != NULL; bind_multiple_spec++) {
        if (strcmp(interface, bind_multiple_spec->interface->name) != 0) continue;

        try_bind_multiple(ctx, id, version, bind_multiple_spec);
        return;
    }

    const wayland_registry_bind_singleton_t * bind_singleton_spec = wayland_registry_bind_singleton;
    for (; bind_singleton_spec->interface != NULL; bind_singleton_spec++) {
        if (strcmp(interface, bind_singleton_spec->interface->name) != 0) continue;

        try_bind_singleton(ctx, id, version, bind_singleton_spec);
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

    wayland_registry_bound_global_t *cur, *next;
    wl_list_for_each_safe(cur, next, &ctx->wl.registry.bound_globals, link) {
        if (cur->id != id) continue;
        if (cur->on_remove == NULL) {
            log_error("wayland::registry::on_registry_remove(): singleton interface %s removed\n", cur->interface->name);
            exit_fail(ctx);
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
    const wayland_registry_bind_singleton_t * spec = wayland_registry_bind_singleton;
    for (; spec->interface != NULL; spec++) {
        if (!spec->required) continue;

        struct wl_proxy ** proxy_ptr = (struct wl_proxy **)((char *)ctx + spec->proxy_offset);
        if (*proxy_ptr == NULL) {
            log_error("wayland::registry::on_sync_callback_done(): required singleton interface %s missing\n", spec->interface->name);
            ctx->wl.registry.initial_sync_had_errors = true;
        }
    }

    ctx->wl.registry.initial_sync_complete = true;
    if (ctx->wl.registry.initial_sync_had_errors) {
        exit_fail(ctx);
    }

    wayland_events_emit_registry_initial_sync(ctx);

    (void)callback_data;
}

static const struct wl_callback_listener sync_callback_listener = {
    .done = on_sync_callback_done,
};

// --- initialization and cleanup ---

void wayland_registry_zero(ctx_t * ctx) {
    // registry handle
    ctx->wl.registry.handle = NULL;

    // bound registry globals
    wl_list_init(&ctx->wl.registry.bound_globals);

    // reset initial sync flags
    ctx->wl.registry.sync_callback = NULL;
    ctx->wl.registry.initial_sync_had_errors = false;
    ctx->wl.registry.initial_sync_complete = false;

    // clear all bound singleton proxies
    const wayland_registry_bind_singleton_t * spec = wayland_registry_bind_singleton;
    for (; spec->interface != NULL; spec++) {
        *(struct wl_proxy **)((char *)ctx + spec->proxy_offset) = NULL;
    }
}

void wayland_registry_init(ctx_t * ctx) {
    // get registry handle
    ctx->wl.registry.handle = wl_display_get_registry(ctx->wl.core.display);
    if (ctx->wl.registry.handle == NULL) {
        log_error("wayland::registry::init(): failed to get registry handle\n");
        exit_fail(ctx);
    }

    wl_registry_add_listener(ctx->wl.registry.handle, &registry_listener, (void *)ctx);

    // perform initial sync
    ctx->wl.registry.sync_callback = wl_display_sync(ctx->wl.core.display);

    wl_callback_add_listener(ctx->wl.registry.sync_callback, &sync_callback_listener, (void *)ctx);
}

void wayland_registry_cleanup(ctx_t * ctx) {
    // destroy sync callback
    if (ctx->wl.registry.sync_callback != NULL) wl_callback_destroy(ctx->wl.registry.sync_callback);

    // destroy bound registry globals
    wayland_registry_bound_global_t *cur, *next;
    wl_list_for_each_safe(cur, next, &ctx->wl.registry.bound_globals, link) {
        wl_list_remove(&cur->link);
        wl_proxy_destroy(cur->proxy);
        free(cur);
    }

    // destroy registry handle
    if (ctx->wl.registry.handle != NULL) wl_registry_destroy(ctx->wl.registry.handle);

    wayland_registry_zero(ctx);
}

// --- public functions ---

bool wayland_registry_is_initial_sync_complete(ctx_t * ctx) {
    return ctx->wl.registry.initial_sync_complete;
}

bool wayland_registry_is_own_proxy(struct wl_proxy * proxy) {
    return wl_proxy_get_tag(proxy) == &proxy_tag;
}
