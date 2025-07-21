#ifndef WL_MIRROR_LOG_H_
#define WL_MIRROR_LOG_H_

#include <stdio.h>

#define wlm_log_debug(ctx, fmt, ...) if ((ctx)->opt.verbose) fprintf(stderr, "debug: " fmt, ##__VA_ARGS__)
#define wlm_log_warn(fmt, ...) fprintf(stderr, "warning: " fmt, ##__VA_ARGS__)
#define wlm_log_error(fmt, ...) fprintf(stderr, "error: " fmt, ##__VA_ARGS__)

#define wlm_assert(expr, fmt, ...) if (!(expr)) wlm_log_error(fmt, ##__VA_ARGS__)

#endif
