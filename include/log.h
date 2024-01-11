#ifndef WL_MIRROR_LOG_H_
#define WL_MIRROR_LOG_H_

#include <stdio.h>

// TODO: check options
#define log_debug(ctx, fmt, ...) if (true) fprintf(stderr, "debug: " fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...) fprintf(stderr, "warning: " fmt, ##__VA_ARGS__)
#define log_error(fmt, ...) fprintf(stderr, "error: " fmt, ##__VA_ARGS__)

#endif
