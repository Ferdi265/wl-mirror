#ifndef WL_MIRROR_MIRROR_XDG_PORTAL_H_
#define WL_MIRROR_MIRROR_XDG_PORTAL_H_

#include <stdint.h>
#include <systemd/sd-bus.h>
#include <pipewire/pipewire.h>

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
    STATE_PW_CREATE_STREAM,
    STATE_RUNNING,
    STATE_BROKEN
} xdg_portal_state_t;

typedef struct xdg_portal_mirror_backend {
    mirror_backend_t header;

    // general info
    uint32_t x, y, w, h;
    uint32_t drm_format;
    uint32_t gl_format;

    // sd-bus state
    screencast_properties_t screencast_properties;
    char * request_handle;
    char * session_handle;
    bool session_open;

    request_ctx_t rctx;
    sd_bus_slot * call_slot;
    sd_bus_slot * session_slot;
    sd_bus * bus;
    event_handler_t dbus_event_handler;

    // pipewire state
    int pw_fd;
    uint32_t pw_node_id;

    struct pw_loop * pw_loop;
    struct pw_context * pw_context;
    struct pw_core * pw_core;
    struct pw_stream * pw_stream;
    struct spa_hook pw_core_listener;
    struct spa_hook pw_stream_listener;
    event_handler_t pw_event_handler;

    xdg_portal_state_t state;
} xdg_portal_mirror_backend_t;

#endif
