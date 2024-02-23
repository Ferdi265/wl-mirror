#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlm/context.h>

#define WLM_LOG_COMPONENT main

void wlm_zero(ctx_t * ctx) {

    wlm_log_zero(ctx);
    wlm_opt_zero(ctx);
    wlm_event_loop_zero(ctx);
    // wlm_stream_zero(ctx);
    wlm_wayland_zero(ctx);
    wlm_egl_zero(ctx);
    // wlm_mirror_zero(ctx);
}

void wlm_init(ctx_t * ctx, int argc, char ** argv) {
    wlm_log(ctx, WLM_TRACE, "initializing");

    wlm_log_init(ctx);

    wlm_opt_parse(ctx, argc, argv);
    (void)argc;
    (void)argv;

    //init_stream(&ctx);

    wlm_event_loop_init(ctx);

    wlm_wayland_init(ctx);

    // egl is initialized in event handlers
    // when wayland is ready

    //init_mirror(&ctx);

    //init_mirror_backend(&ctx);

    wlm_event_loop_run(ctx);
}

void wlm_cleanup(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "cleaning up");

    // wlm_mirror_cleanup(ctx);
    wlm_wayland_cleanup(ctx);
    // wlm_stream_cleanup(ctx);
    wlm_event_loop_cleanup(ctx);
    wlm_opt_cleanup(ctx);
    wlm_log_cleanup(ctx);
}

noreturn void wlm_exit_fail(ctx_t * ctx) {
    wlm_log(ctx, WLM_TRACE, "exiting");

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
