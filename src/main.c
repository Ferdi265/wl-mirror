#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlm/context.h>

void wlm_zero(ctx_t * ctx) {
    // wlm_opt_zero(ctx);
    wlm_log_zero(ctx);
    wlm_event_loop_zero(ctx);
    // wlm_stream_zero(ctx);
    wlm_wayland_zero(ctx);
    wlm_egl_zero(ctx);
    // wlm_mirror_zero(ctx);
}

void wlm_init(ctx_t * ctx, int argc, char ** argv) {
    //parse_opt(&ctx, argc, argv);
    (void)argc;
    (void)argv;

    wlm_log_init(ctx);

    //log_debug(&ctx, "main::init(): initializing stream\n");
    //init_stream(&ctx);

    log_debug(ctx, "main::init(): initializing event system\n");
    wlm_event_loop_init(ctx);

    log_debug(ctx, "main::init(): initializing wayland\n");
    wlm_wayland_init(ctx);

    // egl is initialized in event handlers
    // when wayland is ready

    //log_debug(&ctx, "main::init(): initializing mirror\n");
    //init_mirror(&ctx);

    //log_debug(&ctx, "main::init(): initializing mirror backend\n");
    //init_mirror_backend(&ctx);

    log_debug(ctx, "main::init(): entering event loop\n");
    wlm_event_loop_run(ctx);
    log_debug(ctx, "main::init(): exiting event loop\n");
}

void wlm_cleanup(ctx_t * ctx) {
    log_debug(ctx, "main::cleanup(): deallocating resources\n");

    // wlm_mirror_cleanup(ctx);
    wlm_wayland_cleanup(ctx);
    // wlm_stream_cleanup(ctx);
    wlm_event_loop_cleanup(ctx);
    wlm_log_cleanup(ctx);
    // wlm_opt_cleanup(ctx);
}

noreturn void wlm_exit_fail(ctx_t * ctx) {
    wlm_cleanup(ctx);
    exit(1);
}

int main(int argc, char ** argv) {
    if (argc > 0) {
        // skip program name
        argv++;
        argc--;
    }

    ctx_t ctx;
    wlm_zero(&ctx);
    wlm_init(&ctx, argc, argv);
    wlm_cleanup(&ctx);
}
