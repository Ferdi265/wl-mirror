#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "context.h"
#include "event.h"

void cleanup(ctx_t * ctx) {
    log_debug(ctx, "main::cleanup(): deallocating resources\n");

    wayland_cleanup(ctx);
    event_cleanup(ctx);
}

noreturn void exit_fail(ctx_t * ctx) {
    cleanup(ctx);
    exit(1);
}

int main(int argc, char ** argv) {
    ctx_t ctx;

    event_zero(&ctx);
    wayland_zero(&ctx);

    if (argc > 0) {
        // skip program name
        argv++;
        argc--;
    }

    //parse_opt(&ctx, argc, argv);

    //log_debug(&ctx, "main::main(): initializing stream\n");
    //init_stream(&ctx);

    log_debug(&ctx, "main::main(): initializing event system\n");
    event_init(&ctx);

    log_debug(&ctx, "main::main(): initializing wayland\n");
    wayland_init(&ctx);

    //log_debug(&ctx, "main::main(): initializing EGL\n");
    //init_egl(&ctx);

    //log_debug(&ctx, "main::main(): initializing mirror\n");
    //init_mirror(&ctx);

    //log_debug(&ctx, "main::main(): initializing mirror backend\n");
    //init_mirror_backend(&ctx);

    log_debug(&ctx, "main::main(): entering event loop\n");
    event_loop(&ctx);
    log_debug(&ctx, "main::main(): exiting event loop\n");

    cleanup(&ctx);
}
