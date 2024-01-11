#include "context.h"

void wayland_output_zero(ctx_t * ctx) {
    (void)ctx;
}

void wayland_output_init(ctx_t * ctx) {
    (void)ctx;
}

void wayland_output_cleanup(ctx_t * ctx) {
    (void)ctx;
}

void wayland_output_on_add(ctx_t * ctx, struct wl_output * output) {
    log_debug(ctx, "wayland::output::on_add(): output %p\n", output);

    (void)ctx;
}

void wayland_output_on_remove(ctx_t * ctx, struct wl_output * output) {
    log_debug(ctx, "wayland::output::on_remove(): output %p\n", output);

    (void)ctx;
}
