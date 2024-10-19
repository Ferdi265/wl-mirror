#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <wlm/context.h>

#define ARGS_MIN_CAP 8
static void args_push(ctx_t * ctx, char * arg) {
    if (ctx->stream.args_len == ctx->stream.args_cap) {
        size_t new_cap = ctx->stream.args_cap * 2;
        if (new_cap == 0) new_cap = ARGS_MIN_CAP;

        char ** new_args = realloc(ctx->stream.args, sizeof (char *) * new_cap);
        if (new_args == NULL) {
            wlm_log_error("event::args_push(): failed to grow args array for option stream line\n");
            wlm_exit_fail(ctx);
        }

        // ensure that additionally allocated space is zeroed.
        memset(new_args + sizeof (char *) * ctx->stream.args_cap, 0, sizeof (char *) * (new_cap - ctx->stream.args_cap));

        ctx->stream.args = new_args;
        ctx->stream.args_cap = new_cap;
    }

    ctx->stream.args[ctx->stream.args_len++] = arg;
}

#define LINE_MIN_RESERVE 1024
static void line_reserve(ctx_t * ctx) {
    if (ctx->stream.line_cap - ctx->stream.line_len < LINE_MIN_RESERVE) {
        size_t new_cap = ctx->stream.line_cap * 2;
        if (new_cap == 0) new_cap = LINE_MIN_RESERVE;

        char * new_line = realloc(ctx->stream.line, sizeof (char) * new_cap);
        if (new_line == NULL) {
            wlm_log_error("event::line_reserve(): failed to grow line buffer for option stream line\n");
            wlm_exit_fail(ctx);
        }

        // ensure that additionally allocated space is zeroed.
        memset(new_line + sizeof (char) * ctx->stream.line_cap, 0, sizeof (char) * (new_cap - ctx->stream.line_cap));

        ctx->stream.line = new_line;
        ctx->stream.line_cap = new_cap;
    }
}

enum parse_state {
    BEFORE_ARG,
    ARG_START,
    QUOTED_ARG,
    UNQUOTED_ARG
};
static void on_line(ctx_t * ctx, char * line) {
    char * arg_start = NULL;
    char quote_char = '\0';

    wlm_log_debug(ctx, "event::on_line(): got line '%s'\n", line);

    ctx->stream.args_len = 0;

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
                    args_push(ctx, arg_start);
                    line++;
                    state = BEFORE_ARG;
                }
                break;

            case UNQUOTED_ARG:
                if (!isspace(*line)) {
                    line++;
                } else {
                    *line = '\0';
                    args_push(ctx, arg_start);
                    line++;
                    state = BEFORE_ARG;
                }
                break;
        }
    }

    if (state == QUOTED_ARG) {
        wlm_log_error("event::on_line(): unmatched quote in argument\n");
    }

    if (state == QUOTED_ARG || state == UNQUOTED_ARG) {
        args_push(ctx, arg_start);
    }

    wlm_log_debug(ctx, "event::on_line(): parsed %zd arguments\n", ctx->stream.args_len);
    wlm_opt_parse(ctx, ctx->stream.args_len, ctx->stream.args);

    // clear arguments
    memset(ctx->stream.args, 0, sizeof (char *) * ctx->stream.args_cap);
    ctx->stream.args_len = 0;
}

static void on_stream_data(ctx_t * ctx, uint32_t events) {
    // close the window if the input option stream closed
    if ((events & EPOLLHUP) != 0) wlm_wayland_window_close(ctx);

    // return early if there is nothing to read
    if ((events & EPOLLIN) == 0) return;

    while (true) {
        line_reserve(ctx);

        size_t cap = ctx->stream.line_cap;
        size_t len = ctx->stream.line_len;

        // ensure that last byte is never overwritten to guarantee a 0 terminator
        ssize_t num = read(STDIN_FILENO, ctx->stream.line + len, cap - len - 1);
        if (num == -1 && errno == EWOULDBLOCK) {
            break;
        } else if (num == -1) {
            wlm_log_error("event::on_data(): failed to read data from stdin\n");
            wlm_exit_fail(ctx);
        } else {
            ctx->stream.line_len += num;
        }
    }

    // input might contain multiple '\n' chars
    // each full line that ends in a '\n' should be parsed as options
    //
    // input might also contain null bytes that should be treated as spaces
    char * line = ctx->stream.line;
    size_t len = ctx->stream.line_len;

    // used to track the start of the next line to handle
    char * current_line_start = line;
    for (size_t i = 0; i < len; i++) {
        if (line[i] == '\0') {
            line[i] = ' ';
        } else if (line[i] == '\n') {
            // handle each newline-separated part of the input separately.
            line[i] = '\0';
            on_line(ctx, current_line_start);
            // remember start of next line
            current_line_start = line + i + 1;
        }
    }

    // calculate length of remaining partial line
    size_t current_line_len = (line + len) - current_line_start;
    // move remaining partial line to front
    memmove(line, current_line_start, current_line_len);
    ctx->stream.line_len = current_line_len;
    // clear remaining capacity
    memset(line + current_line_len, 0, ctx->stream.line_cap - current_line_len);
}

void wlm_stream_init(ctx_t * ctx) {
    // initialize context structure
    ctx->stream.line = NULL;
    ctx->stream.line_len = 0;
    ctx->stream.line_cap = 0;

    ctx->stream.args = NULL;
    ctx->stream.args_len = 0;
    ctx->stream.args_cap = 0;

    ctx->stream.event_handler.next = NULL;
    ctx->stream.event_handler.fd = STDIN_FILENO;
    ctx->stream.event_handler.events = EPOLLIN;
    ctx->stream.event_handler.timeout_ms = -1;
    ctx->stream.event_handler.on_event = on_stream_data;
    ctx->stream.event_handler.on_each = NULL;

    if (ctx->opt.stream) {
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        flags |= O_NONBLOCK;
        fcntl(STDIN_FILENO, F_SETFL, flags);

        wlm_event_add_fd(ctx, &ctx->stream.event_handler);
    }

    ctx->stream.initialized = true;
}

void wlm_stream_cleanup(ctx_t * ctx) {
    free(ctx->stream.input);
    free(ctx->stream.args);

    if (ctx->opt.stream) {
        wlm_event_remove_fd(ctx, &ctx->stream.event_handler);
    }
}
