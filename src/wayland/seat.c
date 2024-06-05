#include <stdlib.h>
#include <wlm/context.h>

#define WLM_LOG_COMPONENT wayland

// --- private utility functions ---

static void remove_seat(ctx_t * ctx, wlm_wayland_seat_entry_t * entry) {
    wlm_log(ctx, WLM_DEBUG, "seat %p removed", entry->seat);

    wl_list_remove(&entry->link);
    free(entry);
}

static wlm_wayland_seat_entry_t * find_seat(ctx_t * ctx, struct wl_seat * seat) {
    bool found = false;
    wlm_wayland_seat_entry_t * cur;
    wl_list_for_each(cur, &ctx->wl.seat.seat_list, link) {
        if (cur->seat == seat) {
            found = true;
            break;
        }
    }

    if (!found) {
        wlm_log(ctx, WLM_FATAL, "could not find seat entry for seat %p", seat);
        wlm_exit_fail(ctx);
    }

    return cur;
}

// --- internal event handlers ---

void wlm_wayland_seat_on_add(ctx_t * ctx, struct wl_seat * seat) {
    wlm_wayland_seat_entry_t * cur = malloc(sizeof *cur);
    cur->seat = seat;
    wl_list_insert(&ctx->wl.seat.seat_list, &cur->link);

    wlm_log(ctx, WLM_DEBUG, "seat %p added", seat);
}

void wlm_wayland_seat_on_remove(ctx_t * ctx, struct wl_seat * seat) {
    wlm_wayland_seat_entry_t * entry = find_seat(ctx, seat);
    remove_seat(ctx, entry);
}

// --- initialization and cleanup

void wlm_wayland_seat_zero(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "zeroing");

    wl_list_init(&ctx->wl.seat.seat_list);

    ctx->wl.seat.init_called = false;
    ctx->wl.seat.init_done = false;
}

void wlm_wayland_seat_init(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "initializing");
    wlm_assert(wlm_wayland_registry_is_init_done(ctx), ctx, WLM_FATAL, "initial sync not complete");
    wlm_assert(!wlm_wayland_seat_is_init_called(ctx), ctx, WLM_FATAL, "already initialized");
    ctx->wl.seat.init_called = true;
    ctx->wl.seat.init_done = true;
}

void wlm_wayland_seat_cleanup(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "cleaning up");

    wlm_wayland_seat_entry_t *cur, *next;
    wl_list_for_each_safe(cur, next, &ctx->wl.seat.seat_list, link) {
        remove_seat(ctx, cur);
    }

    wlm_wayland_seat_zero(ctx);
}

// --- public functions ---

bool wlm_wayland_seat_is_init_called(ctx_t * ctx) {
    return ctx->wl.seat.init_called;
}

bool wlm_wayland_seat_is_init_done(ctx_t * ctx) {
    return ctx->wl.seat.init_done;
}
