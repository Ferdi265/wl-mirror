#ifdef WITH_XDG_PORTAL_BACKEND
#include <stdlib.h>
#include <unistd.h>
#include "context.h"
#include "mirror-xdg-portal.h"

static void backend_fail_async(xdg_portal_mirror_backend_t * backend) {
    backend->state = STATE_BROKEN;
}

static char * generate_token() {
    size_t length = strlen("wl_mirror_") + (2 * sizeof (uint32_t)) + 1;
    char * token_buffer = malloc(length);
    if (token_buffer == NULL) {
        return NULL;
    }

    snprintf(token_buffer, length, "wl_mirror_%04x", rand());
    return token_buffer;
}

static char * get_window_handle(ctx_t * ctx) {
    size_t length = ctx->wl.xdg_exported_handle == NULL ? 1 : strlen("wayland:") + strlen(ctx->wl.xdg_exported_handle) + 1;
    char * handle_buffer = malloc(length);
    if (handle_buffer == NULL) {
        return NULL;
    }

    if (ctx->wl.xdg_exported_handle == NULL) {
        handle_buffer[0] = '\0';
        return handle_buffer;
    }

    snprintf(handle_buffer, length, "wayland:%s", ctx->wl.xdg_exported_handle);
    return handle_buffer;
}

// --- dbus method call helpers ---

static int on_request_response(sd_bus_message * reply, void * data, sd_bus_error * err);
static int on_request_reply(sd_bus_message * reply, void * data, sd_bus_error * err) {
    request_ctx_t * rctx = (request_ctx_t *)data;
    ctx_t * ctx = rctx->ctx;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    sd_bus_slot_unref(backend->call_slot);
    backend->call_slot = NULL;

    if (sd_bus_message_is_method_error(reply, NULL)) {
        sd_bus_error_copy(err, sd_bus_message_get_error(reply));
        log_error("mirror-xdg-portal::on_request_reply(): dbus error received: %s, %s\n", err->name, err->name);
        backend_fail_async(backend);
        return 1;
    }

    const char * request_handle = NULL;
    if (sd_bus_message_read_basic(reply, 'o', &request_handle) < 0) {
        log_error("mirror-xdg-portal::on_request_reply(): failed to read reply\n");
        backend_fail_async(backend);
        return 1;
    }

    log_debug(ctx, "mirror-xdg-portal::on_request_reply(): received request handle for %s: %s\n", rctx->name, request_handle);

    if (sd_bus_match_signal_async(backend->bus, &backend->call_slot,
        "org.freedesktop.portal.Desktop",
        request_handle,
        "org.freedesktop.portal.Request", "Response",
        on_request_response, NULL, (void *)rctx
    ) < 0) {
        log_error("mirror-xdg-portal::on_request_reply(): failed to register response listener\n");
        backend_fail_async(backend);
        return 1;
    }

    return 0;
}

static int on_request_response(sd_bus_message * reply, void * data, sd_bus_error * err) {
    request_ctx_t * rctx = (request_ctx_t *)data;
    ctx_t * ctx = rctx->ctx;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    if (sd_bus_message_is_method_error(reply, NULL)) {
        sd_bus_error_copy(err, sd_bus_message_get_error(reply));
        log_error("mirror-xdg-portal::on_request_response(): dbus error received: %s, %s\n", err->name, err->name);
        backend_fail_async(backend);
        return 1;
    }

    uint32_t status;
    if (sd_bus_message_read_basic(reply, 'u', &status) < 0) {
        log_error("mirror-xdg-portal::on_request_response(): failed to read reply\n");
        backend_fail_async(backend);
        return 1;
    } else if (status != 0) {
        log_error("mirror-xdg-portal::on_request_response(): request %s failed: %s\n",
            rctx->name,
            (status == 1) ? "canceled" :
            (status == 2) ? "other error" :
                            "unknown error"
        );
        backend_fail_async(backend);
        return 1;
    }

    request_ctx_handler_t handler = rctx->handler;
    *rctx = (request_ctx_t) {
        .ctx = NULL,
        .name = NULL,
        .handler = NULL
    };

    return handler(ctx, backend, reply);
}

static int on_get_properties_reply(sd_bus_message * reply, void * data, sd_bus_error * err);
static void screencast_get_properties(ctx_t * ctx, xdg_portal_mirror_backend_t * backend) {
    backend->state = STATE_GET_PROPERTIES;
    log_debug(ctx, "mirror-xdg-portal::screencast_get_properties(): getting properties\n");

    if (sd_bus_call_method_async(
        backend->bus, &backend->call_slot,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.DBus.Properties", "GetAll",
        on_get_properties_reply, (void *)ctx,
        "s",
        "org.freedesktop.portal.ScreenCast"
    ) < 0) {
        log_error("mirror-xdg-portal::screencast_get_properties(): failed to call method\n");
        backend_fail_async(backend);
    }
}

static void screencast_create_session(ctx_t * ctx, xdg_portal_mirror_backend_t * backend);
static int on_get_properties_reply(sd_bus_message * reply, void * data, sd_bus_error * err) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    sd_bus_slot_unref(backend->call_slot);
    backend->call_slot = NULL;

    if (sd_bus_message_is_method_error(reply, NULL)) {
        sd_bus_error_copy(err, sd_bus_message_get_error(reply));
        log_error("mirror-xdg-portal::on_get_properties_reply(): dbus error received: %s, %s\n", err->name, err->name);
        backend_fail_async(backend);
        return 1;
    }

    char * keys[3];
    uint32_t values[3];
    if (sd_bus_message_read(reply, "a{sv}", 3,
        &keys[0], "u", &values[0],
        &keys[1], "u", &values[1],
        &keys[2], "u", &values[2]
    ) < 0) {
        log_error("mirror-xdg-portal::on_get_properties_reply(): failed to read reply\n");
        backend_fail_async(backend);
        return 1;
    }

    screencast_properties_t properties = (screencast_properties_t){
        .source_types = 0,
        .cursor_modes = 0,
        .version = 0
    };
    for (int i = 0; i < 3; i++) {
        if (strcmp(keys[i], "AvailableSourceTypes") == 0) properties.source_types = values[i];
        else if (strcmp(keys[i], "AvailableCursorModes") == 0) properties.cursor_modes = values[i];
        else if (strcmp(keys[i], "version") == 0) properties.version = values[i];
        else {
            log_warn("mirror-xdg-portal::on_get_properties_reply(): unknown property: %s\n", keys[i]);
        }
    }

    if (properties.version < SCREENCAST_MIN_VERSION) {
        log_error("mirror-xdg-portal::on_get_properties_reply(): got interface version %d, need at least %d\n",
            properties.version, SCREENCAST_MIN_VERSION
        );
        backend_fail_async(backend);
        return 1;
    }

    log_debug(ctx, "mirror-xdg-portal::on_get_properties_reply(): source types: {%s%s%s }\n",
        (properties.source_types & SCREENCAST_MONITOR) ? " MONITOR," : "",
        (properties.source_types & SCREENCAST_WINDOW) ? " WINDOW," : "",
        (properties.source_types & SCREENCAST_VIRTUAL) ? " VIRTUAL," : ""
    );
    log_debug(ctx, "mirror-xdg-portal::on_get_properties_reply(): cursor modes: {%s%s%s }\n",
        (properties.cursor_modes & SCREENCAST_HIDDEN) ? " HIDDEN," : "",
        (properties.cursor_modes & SCREENCAST_EMBEDDED) ? " EMBEDDED," : "",
        (properties.cursor_modes & SCREENCAST_METADATA) ? " METADATA," : ""
    );
    log_debug(ctx, "mirror-xdg-portal::on_get_properties_reply(): version: %d\n", properties.version);
    backend->screencast_properties = properties;

    screencast_create_session(ctx, backend);

    return 0;
}

static int on_create_session_response(ctx_t * ctx, xdg_portal_mirror_backend_t * backend, sd_bus_message * reply);
static void screencast_create_session(ctx_t * ctx, xdg_portal_mirror_backend_t * backend) {
    backend->state = STATE_CREATE_SESSION;

    char * request_token = generate_token();
    if (request_token == NULL) {
        log_error("mirror-xdg-portal::screencast_create_session(): failed to allocate request token\n");
        backend_fail_async(backend);
        return;
    }

    char * session_token = generate_token();
    if (session_token == NULL) {
        log_error("mirror-xdg-portal::screencast_create_session(): failed to allocate session token\n");
        backend_fail_async(backend);

        free(request_token);
        return;
    }

    log_debug(ctx, "mirror-xdg-portal::screencast_create_session(): creating session with request=%s, session=%s\n",
        request_token, session_token
    );

    backend->rctx = (request_ctx_t) {
        .ctx = ctx,
        .name = "CreateSession",
        .handler = on_create_session_response
    };
    if (sd_bus_call_method_async(backend->bus, &backend->call_slot,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.ScreenCast", "CreateSession",
        on_request_reply, (void *)&backend->rctx,
        "a{sv}", 2,
        "handle_token", "s", request_token,
        "session_handle_token", "s", session_token
    ) < 0) {
        log_error("mirror-xdg-portal::screencast_create_session(): failed to call method\n");
        backend_fail_async(backend);
    }

    free(request_token);
    free(session_token);
}

static int on_session_closed(sd_bus_message * reply, void * data, sd_bus_error * err);
static void screencast_select_sources(ctx_t * ctx, xdg_portal_mirror_backend_t * backend);
static int on_create_session_response(ctx_t * ctx, xdg_portal_mirror_backend_t * backend, sd_bus_message * reply) {
    const char * key;
    const char * session_handle;
    if (sd_bus_message_read(reply, "a{sv}", 1,
        &key, "s", &session_handle
    ) < 0) {
        log_error("mirror-xdg-portal::on_create_session_response(): failed to read reply\n");
        backend_fail_async(backend);
        return 1;
    } else if (strcmp(key, "session_handle") != 0) {
        log_warn("mirror-xdg-portal::on_create_session_response(): unexpected key: %s\n", key);
    }

    log_debug(ctx, "mirror-xdg-portal::on_create_session_response(): got session handle path: %s\n", session_handle);
    backend->session_handle = strdup(session_handle);
    if (backend->session_handle == NULL) {
        log_error("mirror-xdg-portal::on_create_session_response(): failed to allocate session handle path\n");
        backend_fail_async(backend);
        return 1;
    }

    if (sd_bus_match_signal_async(backend->bus, &backend->session_slot,
        "org.freedesktop.portal.Desktop",
        backend->session_handle,
        "org.freedesktop.portal.Session", "Closed",
        on_session_closed, NULL, (void *)ctx
    ) < 0) {
        log_error("mirror-xdg-portal::on_request_reply(): failed to register response listener\n");
        backend_fail_async(backend);
        return 1;
    }

    screencast_select_sources(ctx, backend);
    return 0;
}

static int on_session_closed(sd_bus_message * reply, void * data, sd_bus_error * err) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    if (sd_bus_message_is_method_error(reply, NULL)) {
        sd_bus_error_copy(err, sd_bus_message_get_error(reply));
        log_error("mirror-xdg-portal::on_session_closed(): dbus error received: %s, %s\n", err->name, err->name);
        backend_fail_async(backend);
        return 1;
    }

    log_error("mirror-xdg-portal::on_session_closed(): session closed unexpectedly\n");
    backend_fail_async(backend);
    return 1;
}

static int on_select_sources_response(ctx_t * ctx, xdg_portal_mirror_backend_t * backend, sd_bus_message * reply);
static void screencast_select_sources(ctx_t * ctx, xdg_portal_mirror_backend_t * backend) {
    backend->state = STATE_SELECT_SOURCES;

    char * request_token = generate_token();
    if (request_token == NULL) {
        log_error("mirror-xdg-portal::screencast_select_sources(): failed to allocate request token\n");
        backend_fail_async(backend);
        return;
    }

    log_debug(ctx, "mirror-xdg-portal::screencast_select_sources(): selecting sources with request=%s\n",
        request_token
    );

    uint32_t source_type = 0;
    if (backend->screencast_properties.source_types & SCREENCAST_MONITOR) source_type = SCREENCAST_MONITOR;
    else if (backend->screencast_properties.source_types & SCREENCAST_WINDOW) source_type = SCREENCAST_WINDOW;
    else if (backend->screencast_properties.source_types & SCREENCAST_VIRTUAL) source_type = SCREENCAST_VIRTUAL;
    else {
        log_error("mirror-xdg-portal::screencast_select_sources(): failed to find valid source type\n");
        backend_fail_async(backend);
        free(request_token);
        return;
    }

    uint32_t requested_cursor_mode = (ctx->opt.show_cursor ? SCREENCAST_EMBEDDED : SCREENCAST_HIDDEN);
    uint32_t cursor_mode = 0;
    if (backend->screencast_properties.cursor_modes & requested_cursor_mode) cursor_mode = requested_cursor_mode;
    else if (backend->screencast_properties.source_types & SCREENCAST_EMBEDDED) cursor_mode = SCREENCAST_EMBEDDED;
    else if (backend->screencast_properties.source_types & SCREENCAST_HIDDEN) cursor_mode = SCREENCAST_HIDDEN;
    else {
        log_error("mirror-xdg-portal::screencast_select_sources(): failed to find valid cursor mode\n");
        backend_fail_async(backend);
        free(request_token);
        return;
    }

    backend->rctx = (request_ctx_t) {
        .ctx = ctx,
        .name = "SelectSources",
        .handler = on_select_sources_response
    };
    if (sd_bus_call_method_async(backend->bus, &backend->call_slot,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.ScreenCast", "SelectSources",
        on_request_reply, (void *)&backend->rctx,
        "oa{sv}",
        backend->session_handle,
        3,
        "handle_token", "s", request_token,
        "types", "u", source_type,
        "cursor_mode", "u", cursor_mode
    ) < 0) {
        log_error("mirror-xdg-portal::screencast_select_sources(): failed to call method\n");
        backend_fail_async(backend);
    }

    free(request_token);
}

static void screencast_start(ctx_t * ctx, xdg_portal_mirror_backend_t * backend);
static int on_select_sources_response(ctx_t * ctx, xdg_portal_mirror_backend_t * backend, sd_bus_message * reply) {
    log_debug(ctx, "mirror-xdg-portal::on_select_sources_response(): sources selected successfully\n");
    (void)reply;

    screencast_start(ctx, backend);
    return 0;
}

static int on_start_response(ctx_t * ctx, xdg_portal_mirror_backend_t * backend, sd_bus_message * reply);
static void screencast_start(ctx_t * ctx, xdg_portal_mirror_backend_t * backend) {
    backend->state = STATE_START;

    char * request_token = generate_token();
    if (request_token == NULL) {
        log_error("mirror-xdg-portal::screencast_start(): failed to allocate request token\n");
        backend_fail_async(backend);
        return;
    }

    char * window_handle = get_window_handle(ctx);
    if (window_handle == NULL) {
        log_error("mirror-xdg-portal::screencast_start(): failed to allocate window handle\n");
        backend_fail_async(backend);

        free(request_token);
        return;
    }

    log_debug(ctx, "mirror-xdg-portal::screencast_start(): starting with request=%s, window=%s\n",
        request_token, window_handle
    );

    backend->rctx = (request_ctx_t) {
        .ctx = ctx,
        .name = "Start",
        .handler = on_start_response
    };
    if (sd_bus_call_method_async(backend->bus, &backend->call_slot,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.ScreenCast", "Start",
        on_request_reply, (void *)&backend->rctx,
        "osa{sv}",
        backend->session_handle,
        window_handle,
        1,
        "handle_token", "s", request_token
    ) < 0) {
        log_error("mirror-xdg-portal::screencast_start(): failed to call method\n");
        backend_fail_async(backend);
    }

    free(request_token);
    free(window_handle);
}

static void screencast_open_pipewire(ctx_t * ctx, xdg_portal_mirror_backend_t * backend);
static int on_start_response(ctx_t * ctx, xdg_portal_mirror_backend_t * backend, sd_bus_message * reply) {
    int ret;

    uint32_t pw_node_id;
    int32_t x = -1, y = -1, w = -1, h = -1;

    if (sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "{sv}") < 0) goto fail;
    for (int i = 0; (ret = sd_bus_message_enter_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv")) > 0; i++) {
        const char * result_key;
        if (sd_bus_message_read_basic(reply, 's', &result_key) < 0) goto fail;

        if (strcmp(result_key, "streams") == 0) {
            if (sd_bus_message_enter_container(reply, SD_BUS_TYPE_VARIANT, "a(ua{sv})") < 0) goto fail;
            if (sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(ua{sv})") < 0) goto fail;
            for (int j = 0; (ret = sd_bus_message_enter_container(reply, SD_BUS_TYPE_STRUCT, "ua{sv}")) > 0 && j < 1; j++) {
                if (sd_bus_message_read_basic(reply, 'u', &pw_node_id) < 0) goto fail;
                if (sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "{sv}") < 0) goto fail;
                for (int k = 0; (ret = sd_bus_message_enter_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv")) > 0; k++) {
                    const char * prop_key;
                    if (sd_bus_message_read_basic(reply, 's', &prop_key) < 0) goto fail;

                    if (strcmp(prop_key, "position") == 0) {
                        if (sd_bus_message_read(reply, "v", "(ii)", &x, &y) < 0) goto fail;
                    } else if (strcmp(prop_key, "size") == 0) {
                        if (sd_bus_message_read(reply, "v", "(ii)", &w, &h) < 0) goto fail;
                    } else {
                        if (sd_bus_message_skip(reply, "v") < 0) goto fail;
                    }

                    if (sd_bus_message_exit_container(reply) < 0) goto fail;
                }
                if (ret < 0) goto fail;
                if (sd_bus_message_exit_container(reply) < 0) goto fail;
                if (sd_bus_message_exit_container(reply) < 0) goto fail;
            }
            if (ret < 0) goto fail;
            if (ret > 0) goto too_many;
            if (sd_bus_message_exit_container(reply) < 0) goto fail;
            if (sd_bus_message_exit_container(reply) < 0) goto fail;
        } else {
            if (sd_bus_message_skip(reply, "v") < 0) goto fail;
        }

        if (sd_bus_message_exit_container(reply) < 0) goto fail;
    }
    if (ret < 0) goto fail;
    if (sd_bus_message_exit_container(reply) < 0) goto fail;

    log_debug(ctx, "mirror-xdg-portal::on_start_response(): got pw_node_id=%d, x=%d, y=%d, w=%d, h=%d\n",
        pw_node_id, x, y, w, h
    );

    backend->pw_node_id = pw_node_id;
    backend->x = x;
    backend->y = y;
    backend->w = w;
    backend->h = h;

    screencast_open_pipewire(ctx, backend);
    return 0;

too_many:
    log_error("mirror-xdg-portal::on_start_response(): to many array entries, expected 1\n");
    backend_fail_async(backend);
    return 1;

fail:
    log_error("mirror-xdg-portal::on_start_response(): failed to read reply\n");
    backend_fail_async(backend);
    return 1;
}

static int on_open_pipewire_reply(sd_bus_message * reply, void * data, sd_bus_error * err);
static void screencast_open_pipewire(ctx_t * ctx, xdg_portal_mirror_backend_t * backend) {
    backend->state = STATE_OPEN_PIPEWIRE_REMOTE;

    log_debug(ctx, "mirror-xdg-portal::screencast_open_pipewire(): opening pipewire remote\n");

    if (sd_bus_call_method_async(backend->bus, &backend->call_slot,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.ScreenCast", "OpenPipeWireRemote",
        on_open_pipewire_reply, (void *)ctx,
        "oa{sv}",
        backend->session_handle,
        0
    ) < 0) {
        log_error("mirror-xdg-portal::screencast_open_pipewire(): failed to call method\n");
        backend_fail_async(backend);
    }
}

static void screencast_pipewire_init(ctx_t * ctx, xdg_portal_mirror_backend_t * backend);
static int on_open_pipewire_reply(sd_bus_message * reply, void * data, sd_bus_error * err) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    sd_bus_slot_unref(backend->call_slot);
    backend->call_slot = NULL;

    if (sd_bus_message_is_method_error(reply, NULL)) {
        sd_bus_error_copy(err, sd_bus_message_get_error(reply));
        log_error("mirror-xdg-portal::on_open_pipewire_reply(): dbus error received: %s, %s\n", err->name, err->name);
        backend_fail_async(backend);
        return 1;
    }

    int fd;
    if (sd_bus_message_read_basic(reply, 'h', &fd) < 0) {
        log_error("mirror-xdg-portal::on_open_pipewire_reply(): failed to read reply\n");
        backend_fail_async(backend);
        return 1;
    }

    log_debug(ctx, "mirror-xdg-portal::on_open_pipewire_reply(): received pipewire fd: %d\n", fd);
    backend->pw_fd = fd;

    screencast_pipewire_init(ctx, backend);
    return 0;
}

// --- pipewire functions ---

static void screencast_pipewire_create_stream(ctx_t * ctx, xdg_portal_mirror_backend_t * backend);
static void on_pw_core_info(void * data, const struct pw_core_info * info);
static void on_pw_core_done(void * data, uint32_t id, int seq);
static void on_pw_core_error(void * data, uint32_t id, int seq, int res, const char * msg);

static const struct pw_core_events pw_core_events = {
    .version = PW_VERSION_CORE_EVENTS,
    .info = on_pw_core_info,
    .done = on_pw_core_done,
    .error = on_pw_core_error
};

static void screencast_pipewire_init(ctx_t * ctx, xdg_portal_mirror_backend_t * backend) {
    backend->state = STATE_PW_INIT;

    log_debug(ctx, "mirror-xdg-portal::screencast_pipewire_init(): initializing pipewire event loop\n");

    backend->pw_core = pw_context_connect_fd(backend->pw_context, backend->pw_fd, NULL, 0);
    backend->pw_fd = -1;
    if (backend->pw_core == NULL) {
        log_error("mirror-xdg-portal::screencast_pipewire_init(): failed to create pipewire core\n");
        backend_fail_async(backend);
        return;
    }

    // add core event listener for info callback to check version
    pw_core_add_listener(backend->pw_core, &backend->pw_core_listener, &pw_core_events, (void *)ctx);
}

static void on_pw_core_info(void * data, const struct pw_core_info * info) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    // TODO: actually check versions
    log_debug(ctx, "mirror-xdg-portal::on_pw_core_info(): got core info: %p\n", info);

    screencast_pipewire_create_stream(ctx, backend);
}

static void on_pw_core_done(void * data, uint32_t id, int seq) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    // TODO: actually do something with the serial
    log_debug(ctx, "mirror-xdg-portal::on_pw_core_done(): got core info: id = %d, seq = %d\n", id, seq);

    (void)backend;
}

static void on_pw_core_error(void * data, uint32_t id, int seq, int res, const char * msg) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    // TODO: do something more useful with the error
    log_error("mirror-xdg-portal::on_pw_core_done(): got error: id = %d, seq = %d, res = %d, msg = %s\n", id, seq, res, msg);
    backend_fail_async(backend);
}


static void on_pw_renegotiate(void * data, uint64_t expirations);
static void on_pw_process(void * data);
static void on_pw_param_changed(void * data, uint32_t id, const struct spa_pod * param);
static void on_pw_state_changed(void * data, enum pw_stream_state old, enum pw_stream_state new, const char * error);

static const struct pw_stream_events pw_stream_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .state_changed = on_pw_state_changed,
    .param_changed = on_pw_param_changed,
    .process = on_pw_process
};

static void screencast_pipewire_create_stream(ctx_t * ctx, xdg_portal_mirror_backend_t * backend) {
    backend->state = STATE_PW_CREATE_STREAM;

    log_debug(ctx, "mirror-xdg-portal::screencast_pipewire_create_stream(): creating video stream\n");

    backend->pw_renegotiate = pw_loop_add_event(backend->pw_loop, on_pw_renegotiate, (void *)ctx);

    backend->pw_stream = pw_stream_new(
        backend->pw_core, "wl-mirror",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Video",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Screen",
            NULL
        )
    );

    pw_stream_add_listener(backend->pw_stream, &backend->pw_stream_listener, &pw_stream_events, (void *)ctx);

    // TODO: create stream parameters pod
    const struct spa_pod ** params = NULL;
    uint32_t num_params = 0;

    // TODO: actually create params before doing this
    //pw_stream_connect(
    //    backend->pw_stream, PW_DIRECTION_INPUT, backend->pw_node_id,
    //    PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS,
    //    params, num_params
    //);
}

static void on_pw_renegotiate(void * data, uint64_t expirations) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    // TODO: actually do something here
    log_debug(ctx, "mirror-xdg-portal::on_pw_renegotiate(): need to renegotiate, expirations = %ld\n", expirations);

    (void)backend;
}

static void on_pw_process(void * data) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    // TODO: actually do something here
    log_debug(ctx, "mirror-xdg-portal::on_pw_process(): new video data\n");

    (void)backend;
}

static void on_pw_param_changed(void * data, uint32_t id, const struct spa_pod * param) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    // TODO: actually do something here
    log_debug(ctx, "mirror-xdg-portal::on_pw_param_changed(): id = %d, param = %p\n", id, param);

    (void)backend;
}

static void on_pw_state_changed(void * data, enum pw_stream_state old, enum pw_stream_state new, const char * error) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    // TODO: actually do something here
    log_debug(ctx, "mirror-xdg-portal::on_pw_state_changed(): old = %d, new = %d, error = %s\n", old, new, error);

    (void)backend;
}

// --- backend event handlers ---

static void do_capture(ctx_t * ctx) {
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    if (backend->state == STATE_IDLE) {
        screencast_get_properties(ctx, backend);
    } else if (backend->state == STATE_RUNNING) {

    } else if (backend->state == STATE_BROKEN) {
        backend_fail(ctx);
    }
}

static void do_cleanup(ctx_t * ctx) {
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    log_debug(ctx, "mirror-xdg-portal::do_cleanup(): destroying mirror-xdg-portal objects\n");

    // deregister event handlers
    if (backend->bus != NULL) event_remove_fd(ctx, &backend->dbus_event_handler);
    if (backend->pw_loop != NULL) event_remove_fd(ctx, &backend->pw_event_handler);

    // stop the pipewire event loop before releasing resources
    if (backend->pw_loop != NULL) pw_loop_leave(backend->pw_loop);

    // release pipewire resources
    if (backend->pw_stream != NULL) pw_stream_destroy(backend->pw_stream);
    if (backend->pw_core != NULL) pw_core_disconnect(backend->pw_core);
    if (backend->pw_context != NULL) pw_context_destroy(backend->pw_context);
    if (backend->pw_loop != NULL) pw_loop_destroy(backend->pw_loop);

    // release sd-bus resources
    // FIXME: maybe call close on session
    if (backend->pw_fd != -1) close(backend->pw_fd);
    if (backend->session_slot != NULL) sd_bus_slot_unref(backend->session_slot);
    if (backend->call_slot != NULL) sd_bus_slot_unref(backend->call_slot);
    if (backend->bus != NULL) sd_bus_unref(backend->bus);

    // deinitialize pipewire
    pw_deinit();

    free(backend);
    ctx->mirror.backend = NULL;
}

// --- loop event handlers ---

static bool update_bus_events(xdg_portal_mirror_backend_t * backend) {
    if ((backend->dbus_event_handler.fd = sd_bus_get_fd(backend->bus)) < 0) {
        return false;
    }

    int events;
    if ((events = sd_bus_get_events(backend->bus)) < 0) {
        return false;
    }

    // TODO: don't assume POLL and EPOLL event constants are the same
    backend->dbus_event_handler.events = events;

    uint64_t timeout;
    if (sd_bus_get_timeout(backend->bus, &timeout) < 0) {
        return false;
    }

    if (timeout == UINT64_MAX) {
        backend->dbus_event_handler.timeout_ms = -1;
    } else {
        backend->dbus_event_handler.timeout_ms = timeout / 1000;
    }

    return true;
}

static void on_loop_dbus_event(ctx_t * ctx) {
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;
    int ret;

    // process dbus events
    while ((ret = sd_bus_process(backend->bus, NULL) > 0));

    if (ret < 0) {
        log_error("mirror-xdg-portal::on_loop_pw_event(): failed to process dbus events\n");
        backend_fail(ctx);
    }
}

static void on_loop_dbus_each(ctx_t * ctx) {
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    int old_fd = backend->dbus_event_handler.fd;
    if (!update_bus_events(backend)) {
        log_error("mirror-xdg-portal::on_loop_dbus_each(): failed to update dbus pollfd\n");
        backend_fail(ctx);
    }

    int new_fd = backend->dbus_event_handler.fd;
    if (old_fd != new_fd) {
        event_handler_t handler;
        handler.fd = old_fd;

        event_remove_fd(ctx, &handler);
        event_add_fd(ctx, &backend->dbus_event_handler);
    } else {
        event_change_fd(ctx, &backend->dbus_event_handler);
    }
}

static void on_loop_pw_event(ctx_t * ctx) {
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    if (pw_loop_iterate(backend->pw_loop, -1) < 0) {
        log_error("mirror-xdg-portal::on_loop_pw_event(): failed to process pipewire events\n");
        backend_fail(ctx);
    }
}

// --- init_mirror_xdg_portal ---

void init_mirror_xdg_portal(ctx_t * ctx) {
    // check for required protocols
    if (ctx->wl.xdg_exporter == NULL) {
        // TODO: this should not be fatal
        log_error("mirror-xdg-portal::init(): missing xdg_foreign protocol\n");
        return;
    }

    // allocate backend context structure
    xdg_portal_mirror_backend_t * backend = malloc(sizeof (xdg_portal_mirror_backend_t));
    if (backend == NULL) {
        log_error("mirror-xdg-portal::init(): failed to allocate backend state\n");
        return;
    }

    // initialize context structure
    backend->header.do_capture = do_capture;
    backend->header.do_cleanup = do_cleanup;
    backend->header.fail_count = 0;

    // general info
    backend->x = 0;
    backend->y = 0;
    backend->w = 0;
    backend->h = 0;

    // sd-bus state
    backend->screencast_properties = (screencast_properties_t) {
        .source_types = 0,
        .cursor_modes = 0,
        .version = 0
    };
    backend->session_handle = NULL;

    backend->rctx = (request_ctx_t) {
        .ctx = NULL,
        .name = NULL,
        .handler = NULL
    };

    backend->call_slot = NULL;
    backend->session_slot = NULL;
    backend->bus = NULL;
    backend->dbus_event_handler.fd = -1;
    backend->dbus_event_handler.timeout_ms = -1;
    backend->dbus_event_handler.events = 0;
    backend->dbus_event_handler.on_event = on_loop_dbus_event;
    backend->dbus_event_handler.on_each = on_loop_dbus_each;
    backend->dbus_event_handler.next = NULL;

    // pipewire state
    backend->pw_fd = -1;
    backend->pw_node_id = 0;

    backend->pw_loop = NULL;
    backend->pw_context = NULL;
    backend->pw_core = NULL;
    backend->pw_stream = NULL;
    backend->pw_renegotiate = NULL;
    backend->pw_event_handler.fd = -1;
    backend->pw_event_handler.timeout_ms = -1;
    backend->pw_event_handler.events = 0;
    backend->pw_event_handler.on_event = on_loop_pw_event;
    backend->pw_event_handler.on_each = NULL;
    backend->pw_event_handler.next = NULL;

    backend->state = STATE_IDLE;

    // initialize pipewire
    pw_init(NULL, NULL);

    // set backend object as current backend
    ctx->mirror.backend = (mirror_backend_t *)backend;

    if (sd_bus_default_user(&backend->bus) < 0) {
        log_error("mirror-xdg-portal::init(): failed to get DBus session bus\n");
        backend_fail(ctx);
        return;
    }

    if (!update_bus_events(backend)) {
        log_error("mirror-xdg-portal::init(): failed to update DBus epoll events\n");

        // clean this up here, otherwise unknown if event listener registered
        sd_bus_unref(backend->bus);
        backend->bus = NULL;

        backend_fail(ctx);
        return;
    }

    backend->pw_loop = pw_loop_new(NULL);
    if (backend->pw_loop == NULL) {
        log_error("mirror-xdg-portal::init(): failed to create pipewire event loop\n");

        backend_fail(ctx);
        return;
    }

    backend->pw_event_handler.fd = pw_loop_get_fd(backend->pw_loop);
    backend->pw_event_handler.events = EPOLLIN;
    // start the pipewire event loop
    pw_loop_enter(backend->pw_loop);

    backend->pw_context = pw_context_new(backend->pw_loop, NULL, 0);
    if (backend->pw_context == NULL) {
        log_error("mirror-xdg-portal::init(): failed to create pipewire context\n");

        backend_fail(ctx);
        return;
    }

    event_add_fd(ctx, &backend->dbus_event_handler);
    event_add_fd(ctx, &backend->pw_event_handler);
}

#endif
