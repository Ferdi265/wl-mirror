#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include "context.h"

void init_event(ctx_t * ctx) {
    ctx->event.line = NULL;
    ctx->event.line_len = 0;
    ctx->event.line_cap = 0;

    ctx->event.args = NULL;
    ctx->event.args_len = 0;
    ctx->event.args_cap = 0;
}

void cleanup_event(ctx_t * ctx) {
    free(ctx->event.line);
    free(ctx->event.args);
}

#define ARGS_MIN_CAP 8
void event_args_push(ctx_t * ctx, char * arg) {
    if (ctx->event.args_len == ctx->event.args_cap) {
        size_t new_cap = ctx->event.args_cap * 2;
        if (new_cap == 0) new_cap = ARGS_MIN_CAP;

        char ** new_args = realloc(ctx->event.args, sizeof (char *) * new_cap);
        if (new_args == NULL) {
            log_error("event_args_push: failed to grow args array for option stream line\n");
            exit_fail(ctx);
        }

        ctx->event.args = new_args;
        ctx->event.args_cap = new_cap;
    }

    ctx->event.args[ctx->event.args_len++] = arg;
}

#define LINE_MIN_RESERVE 1024
void event_line_reserve(ctx_t * ctx) {
    if (ctx->event.line_cap - ctx->event.line_len < LINE_MIN_RESERVE) {
        size_t new_cap = ctx->event.line_cap * 2;
        if (new_cap == 0) new_cap = LINE_MIN_RESERVE;

        char * new_line = realloc(ctx->event.line, sizeof (char) * new_cap);
        if (new_line == NULL) {
            log_error("event_line_reserve: failed to grow line buffer for option stream line\n");
            exit_fail(ctx);
        }

        ctx->event.line = new_line;
        ctx->event.line_cap = new_cap;
    }
}

enum parse_state {
    BEFORE_ARG,
    ARG_START,
    QUOTED_ARG,
    UNQUOTED_ARG
};
void event_stdin_line(ctx_t * ctx, char * line) {
    char * arg_start = NULL;
    char quote_char = '\0';

    log_debug(ctx, "event_stdin_line: got line '%s'\n", line);

    ctx->event.args_len = 0;

    enum parse_state state = BEFORE_ARG;
    while (*line != '\0') {
        switch (state) {
            case BEFORE_ARG:
                if (isspace(*line)) {
                    line++;
                } else {
                    state = ARG_START;
                }
                break;

            case ARG_START:
                if (*line == '"' || *line == '\'') {
                    quote_char = *line;
                    line++;

                    arg_start = line;
                    state = QUOTED_ARG;
                } else {
                    arg_start = line;
                    state = UNQUOTED_ARG;
                }
                break;

            case QUOTED_ARG:
                if (*line != quote_char) {
                    line++;
                } else {
                    *line = '\0';
                    event_args_push(ctx, arg_start);
                    line++;
                    state = BEFORE_ARG;
                }
                break;

            case UNQUOTED_ARG:
                if (!isspace(*line)) {
                    line++;
                } else {
                    *line = '\0';
                    event_args_push(ctx, arg_start);
                    line++;
                    state = BEFORE_ARG;
                }
                break;
        }
    }

    if (state == QUOTED_ARG) {
        log_error("event_stdin_line: unmatched quote in argument\n");
    }

    if (state == QUOTED_ARG || state == UNQUOTED_ARG) {
        event_args_push(ctx, arg_start);
    }

    log_debug(ctx, "event_stdin_line: parsed %zd arguments\n", ctx->event.args_len);

    parse_opt(ctx, ctx->event.args_len, ctx->event.args);
}

void event_stdin_data(ctx_t * ctx) {
    while (true) {
        event_line_reserve(ctx);

        size_t cap = ctx->event.line_cap;
        size_t len = ctx->event.line_len;
        ssize_t num = read(STDIN_FILENO, ctx->event.line + len, cap - len);
        if (num == -1 && errno == EWOULDBLOCK) {
            break;
        } else if (num == -1) {
            log_error("event_stdin_data: failed to read data from stdin\n");
            exit_fail(ctx);
        } else {
            ctx->event.line_len += num;
        }
    }

    char * line = ctx->event.line;
    size_t len = ctx->event.line_len;
    for (size_t i = 0; i < len; i++) {
        if (line[i] == '\0') {
            line[i] = ' ';
        } else if (line[i] == '\n') {
            line[i] = '\0';
            event_stdin_line(ctx, line);
            memmove(line, line + (i + 1), len - (i + 1));
            ctx->event.line_len -= i + 1;
            break;
        }
    }
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
