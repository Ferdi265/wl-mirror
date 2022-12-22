#ifndef WL_MIRROR_MIRROR_XDG_PORTAL_H_
#define WL_MIRROR_MIRROR_XDG_PORTAL_H_

#include <stdint.h>
#include <systemd/sd-bus.h>

#include "event.h"
#include "mirror.h"

struct xdg_portal_mirror_backend;

typedef enum {
    SCREENCAST_MONITOR = 1,
    SCREENCAST_WINDOW = 2,
    SCREENCAST_VIRTUAL = 4
} screencast_source_types_t;

typedef enum {
    SCREENCAST_HIDDEN = 1,
    SCREENCAST_EMBEDDED = 2,
    SCREENCAST_METADATA = 4
} screencast_cursor_modes_t;

typedef struct {
    screencast_source_types_t source_types;
    screencast_cursor_modes_t cursor_modes;
    uint32_t version;
} screencast_properties_t;

typedef int (*request_ctx_handler_t)(struct ctx * ctx, struct xdg_portal_mirror_backend * backend, sd_bus_message * reply);

typedef struct {
    struct ctx * ctx;
    const char * name;
    request_ctx_handler_t handler;
} request_ctx_t;

#define SCREENCAST_MIN_VERSION 2

typedef enum {
    STATE_IDLE,
    STATE_GET_PROPERTIES,
    STATE_CREATE_SESSION,
    STATE_SELECT_SOURCES,
    STATE_START,
    STATE_OPEN_PIPEWIRE_REMOTE,
    STATE_PW_INIT,
    STATE_RUNNING,
    STATE_BROKEN
} xdg_portal_state_t;

typedef struct xdg_portal_mirror_backend {
    mirror_backend_t header;

    sd_bus * bus;
    event_handler_t event_handler;

    screencast_properties_t screencast_properties;
    char * session_handle;

    int pw_fd;
    uint32_t pw_node_id;
    uint32_t x, y, w, h;

    request_ctx_t rctx;
    sd_bus_slot * call_slot;
    sd_bus_slot * session_slot;
    xdg_portal_state_t state;
} xdg_portal_mirror_backend_t;

#endif
