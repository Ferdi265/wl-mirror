#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include "event.h"

void event_stdin_line(ctx_t * ctx, char * line) {

}

void event_stdin_data(ctx_t * ctx) {

}

void event_loop(ctx_t * ctx) {
    struct pollfd fds[2];
    fds[0].fd = wl_display_get_fd(ctx->wl.display);
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    if (!ctx->opt.stream) {
        // disable polling for stdin if stream mode not enabled
        fds[1].fd = -1;
    }

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(STDIN_FILENO, F_SETFL, flags);

    while (poll(fds, 2, -1) >= 0 && !ctx->wl.closing) {
        if (fds[0].revents & POLLIN) {
            if (wl_display_dispatch(ctx->wl.display) == -1) {
                ctx->wl.closing = true;
            }
        }

        if (fds[1].revents & POLLIN) {
            event_stdin_data(ctx);
        }

        fds[0].revents = 0;
        fds[1].revents = 0;
        wl_display_flush(ctx->wl.display);
    }
}
