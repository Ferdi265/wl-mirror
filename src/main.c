#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlm/context.h>
#include <wlm/event.h>

void wlm_cleanup(ctx_t * ctx) {
    wlm_log_debug(ctx, "main::cleanup(): deallocating resources\n");

    if (ctx->mirror.initialized) wlm_mirror_cleanup(ctx);
    if (ctx->egl.initialized) wlm_egl_cleanup(ctx);
    if (ctx->wl.initialized) wlm_wayland_cleanup(ctx);
    if (ctx->stream.initialized) wlm_stream_cleanup(ctx);
    if (ctx->event.initialized) wlm_event_cleanup(ctx);

    wlm_cleanup_opt(ctx);
}

noreturn void wlm_exit_fail(ctx_t * ctx) {
    wlm_cleanup(ctx);
    exit(1);
}

int main(int argc, char ** argv) {
    ctx_t ctx = { 0 };

    ctx.event.initialized = false;
    ctx.stream.initialized = false;
    ctx.wl.initialized = false;
    ctx.egl.initialized = false;
    ctx.mirror.initialized = false;

    wlm_opt_init(&ctx);
    wlm_event_init(&ctx);

    if (argc > 0) {
        // skip program name
        argv++;
        argc--;
    }

    wlm_opt_parse(&ctx, argc, argv);

    wlm_log_debug(&ctx, "main::main(): initializing stream\n");
    wlm_stream_init(&ctx);

    wlm_log_debug(&ctx, "main::main(): initializing wayland\n");
    wlm_wayland_init(&ctx);

    wlm_log_debug(&ctx, "main::main(): initializing EGL\n");
    wlm_egl_init(&ctx);

    wlm_log_debug(&ctx, "main::main(): configuring wayland window\n");
    wlm_wayland_configure_window(&ctx);

    wlm_log_debug(&ctx, "main::main(): initializing mirror\n");
    wlm_mirror_init(&ctx);

    wlm_log_debug(&ctx, "main::main(): initializing mirror backend\n");
    wlm_mirror_backend_init(&ctx);

    wlm_log_debug(&ctx, "main::main(): entering event loop\n");
    wlm_event_loop(&ctx);
    wlm_log_debug(&ctx, "main::main(): exiting event loop\n");

    wlm_cleanup(&ctx);
}
