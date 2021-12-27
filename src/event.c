#include <unistd.h>
#include <poll.h>
#include "event.h"

void event_loop(ctx_t * ctx) {
    struct pollfd fds[2];
    fds[0].fd = wl_display_get_fd(ctx->wl.display);
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    while (poll(fds, 2, -1) >= 0 && !ctx->wl.closing) {
        if (fds[0].revents & POLLIN) {
            if (wl_display_dispatch(ctx->wl.display) == -1) {
                ctx->wl.closing = true;
            }
        }

        if (fds[1].revents & POLLIN) {
            // TODO: handle stdin events
        }

        fds[0].revents = 0;
        fds[1].revents = 0;
        wl_display_flush(ctx->wl.display);
    }
}
