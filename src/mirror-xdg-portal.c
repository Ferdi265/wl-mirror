#ifdef WITH_XDG_PORTAL_BACKEND
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include <spa/pod/dynamic.h>
#include <spa/param/format-utils.h>
#include <spa/param/video/raw-utils.h>
#include <libdrm/drm_fourcc.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "context.h"
#include "mirror-xdg-portal.h"

#define ARRAY_LENGTH(arr) ((sizeof ((arr))) / (sizeof ((arr)[0])))

static void backend_fail_async(xdg_portal_mirror_backend_t * backend) {
    backend->state = STATE_BROKEN;
}

typedef struct {
    uint32_t spa_format;
    uint32_t drm_format;
    GLint gl_format;
} spa_drm_gl_format_t;

static const spa_drm_gl_format_t spa_drm_gl_formats[] = {
    {
        .spa_format = SPA_VIDEO_FORMAT_BGRA,
        .drm_format = DRM_FORMAT_ARGB8888,
        .gl_format = GL_BGRA_EXT,
    },
    {
        .spa_format = SPA_VIDEO_FORMAT_RGBA,
        .drm_format = DRM_FORMAT_ABGR8888,
        .gl_format = GL_RGBA,
    },
    {
        .spa_format = SPA_VIDEO_FORMAT_BGRx,
        .drm_format = DRM_FORMAT_XRGB8888,
        .gl_format = GL_BGR_EXT,
    },
    {
        .spa_format = SPA_VIDEO_FORMAT_RGBx,
        .drm_format = DRM_FORMAT_XBGR8888,
        .gl_format = GL_RGB,
    },
    {
        .spa_format = -1U,
        .drm_format = -1U,
        .gl_format = -1U
    }
};

static const spa_drm_gl_format_t * spa_drm_gl_format_from_spa(uint32_t spa_format) {
    const spa_drm_gl_format_t * format = spa_drm_gl_formats;
    while (format->spa_format != -1U) {
        if (format->spa_format == spa_format) {
            return format;
        }

        format++;
    }

    return NULL;
}

static int token_counter;
static char * generate_token() {
    int pid = getpid();
    int id = ++token_counter;

    char * token_buffer;
    int status = asprintf(&token_buffer, "wl_mirror_%d_%d", pid, id);
    if (status == -1) {
        return NULL;
    }

    return token_buffer;
}

static char * get_unique_name_identifier(xdg_portal_mirror_backend_t * backend) {
    const char * unique_name;
    sd_bus_get_unique_name(backend->bus, &unique_name);
    if (unique_name[0] != ':') {
        return NULL;
    }

    size_t length = strlen(unique_name);
    char * ident_buffer = malloc(length);
    if (ident_buffer == NULL) {
        return NULL;
    }

    strncpy(ident_buffer, unique_name + 1, length);
    for (size_t i = 0; i < length - 1; i++) {
        if (ident_buffer[i] == '.') {
            ident_buffer[i] = '_';
        }
    }

    return ident_buffer;
}

static char * generate_handle(xdg_portal_mirror_backend_t * backend, const char * prefix, const char * token) {
    char * ident = get_unique_name_identifier(backend);
    if (ident == NULL) {
        return NULL;
    }

    char * handle_buffer;
    int status = asprintf(&handle_buffer, "%s/%s/%s", prefix, ident, token);
    if (status == -1) {
        free(ident);
        return NULL;
    }

    free(ident);
    return handle_buffer;
}

static char * get_window_handle(ctx_t * ctx) {
    const char * prefix = ctx->wl.xdg_exported_handle != NULL ? "wayland:" : "";
    const char * name = ctx->wl.xdg_exported_handle != NULL ? ctx->wl.xdg_exported_handle : "";

    char * handle_buffer;
    int status = asprintf(&handle_buffer, "%s%s", prefix, name);
    if (status == -1) {
        return NULL;
    }

    return handle_buffer;
}

// --- dbus method call helpers ---

static int on_request_response(sd_bus_message * reply, void * data, sd_bus_error * err);
static char * register_new_request(ctx_t * ctx, xdg_portal_mirror_backend_t * backend) {
    (void)ctx;

    char * token = generate_token();
    if (token == NULL) {
        log_error("mirror-xdg-portal::reqister_new_request(): failed to allocate request token\n");
        return NULL;
    }

    char * request_handle = generate_handle(backend, "/org/freedesktop/portal/desktop/request", token);
    if (request_handle == NULL) {
        log_error("mirror-xdg-portal::reqister_new_request(): failed to allocate request handle\n");
        free(token);
        return NULL;
    }

    if (sd_bus_match_signal_async(backend->bus, &backend->call_slot,
        "org.freedesktop.portal.Desktop",
        request_handle,
        "org.freedesktop.portal.Request", "Response",
        on_request_response, NULL, (void *)&backend->rctx
    ) < 0) {
        log_error("mirror-xdg-portal::reqister_new_request(): failed to register response listener\n");
        free(token);
        free(request_handle);
        return NULL;
    }

    backend->request_handle = request_handle;
    return token;
}

static int on_session_closed(sd_bus_message * reply, void * data, sd_bus_error * err);
static char * register_new_session(ctx_t * ctx, xdg_portal_mirror_backend_t * backend) {
    char * token = generate_token();
    if (token == NULL) {
        log_error("mirror-xdg-portal::reqister_new_session(): failed to allocate session token\n");
        return NULL;
    }

    char * session_handle = generate_handle(backend, "/org/freedesktop/portal/desktop/session", token);
    if (session_handle == NULL) {
        log_error("mirror-xdg-portal::reqister_new_session(): failed to allocate session handle\n");
        free(token);
        return NULL;
    }

    if (sd_bus_match_signal_async(backend->bus, &backend->session_slot,
        "org.freedesktop.portal.Desktop",
        backend->session_handle,
        "org.freedesktop.portal.Session", "Closed",
        on_session_closed, NULL, (void *)ctx
    ) < 0) {
        log_error("mirror-xdg-portal::reqister_new_session(): failed to register session closed listener\n");
        free(token);
        free(session_handle);
        return NULL;
    }

    backend->session_handle = session_handle;
    return token;
}

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

    if (backend->request_handle == NULL) {
        log_error("mirror-xdg-portal::on_request_reply(): no ongoing request\n");
        backend_fail_async(backend);
        return 1;
    }

    if (strcmp(backend->request_handle, request_handle) != 0) {
        log_error("mirror-xdg-portal::on_request_reply(): request handles differ: expected %s, got %s\n",
            backend->request_handle, request_handle
        );
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

    if (backend->request_handle == NULL) {
        log_error("mirror-xdg-portal::on_request_response(): no ongoing request\n");
        backend_fail_async(backend);
        return 1;
    }

    free(backend->request_handle);
    backend->request_handle = NULL;

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

static int on_session_closed(sd_bus_message * reply, void * data, sd_bus_error * err);
static int on_create_session_response(ctx_t * ctx, xdg_portal_mirror_backend_t * backend, sd_bus_message * reply);
static void screencast_create_session(ctx_t * ctx, xdg_portal_mirror_backend_t * backend) {
    backend->state = STATE_CREATE_SESSION;

    char * request_token = register_new_request(ctx, backend);
    if (request_token == NULL) {
        log_error("mirror-xdg-portal::screencast_create_session(): failed to register request\n");
        backend_fail_async(backend);
        return;
    }

    char * session_token = register_new_session(ctx, backend);
    if (session_token == NULL) {
        log_error("mirror-xdg-portal::screencast_create_session(): failed to register session\n");
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

        free(request_token);
        free(session_token);
        return;
    }

    free(request_token);
    free(session_token);
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

    backend->session_open = false;

    log_error("mirror-xdg-portal::on_session_closed(): session closed unexpectedly\n");
    backend_fail_async(backend);
    return 1;
}

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

    if (backend->session_handle == NULL) {
        log_error("mirror-xdg-portal::on_create_session_response(): no ongoing session\n");
        backend_fail_async(backend);
        return 1;
    }

    if (strcmp(backend->session_handle, session_handle) != 0) {
        log_error("mirror-xdg-portal::on_create_session_response(): session handles differ: expected %s, got %s\n",
            backend->session_handle, session_handle
        );
        backend_fail_async(backend);
        return 1;
    }

    backend->session_open = true;

    screencast_select_sources(ctx, backend);
    return 0;
}

static int on_select_sources_response(ctx_t * ctx, xdg_portal_mirror_backend_t * backend, sd_bus_message * reply);
static void screencast_select_sources(ctx_t * ctx, xdg_portal_mirror_backend_t * backend) {
    backend->state = STATE_SELECT_SOURCES;

    char * request_token = register_new_request(ctx, backend);
    if (request_token == NULL) {
        log_error("mirror-xdg-portal::screencast_select_sources(): failed to register request\n");
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

        free(request_token);
        return;
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

    char * request_token = register_new_request(ctx, backend);
    if (request_token == NULL) {
        log_error("mirror-xdg-portal::screencast_start(): failed to register request\n");
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

        free(request_token);
        free(window_handle);
        return;
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

    backend->pw_fd = dup(fd);
    if (backend->pw_fd == -1) {
        log_error("mirror-xdg-portal::on_open_pipewire_reply(): failed to duplicate pipewire fd\n");
    }

    log_debug(ctx, "mirror-xdg-portal::on_open_pipewire_reply(): received pipewire fd: %d\n", backend->pw_fd);

    screencast_pipewire_init(ctx, backend);
    return 0;
}

// --- pipewire functions ---

static void on_pw_core_info(void * data, const struct pw_core_info * info);
static void on_pw_core_error(void * data, uint32_t id, int seq, int res, const char * msg);

static const struct pw_core_events pw_core_events = {
    .version = PW_VERSION_CORE_EVENTS,
    .info = on_pw_core_info,
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

    log_debug(ctx, "mirror-xdg-portal::screencast_pipewire_init(): pipewire core created\n");

}

static void screencast_pipewire_create_stream(ctx_t * ctx, xdg_portal_mirror_backend_t * backend);
static void on_pw_core_info(void * data, const struct pw_core_info * info) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    log_debug(ctx, "mirror-xdg-portal::on_pw_core_info(): pipewire version = %s\n", info->version);

    uint32_t major, minor, patch;
    if (sscanf(info->version, "%d.%d.%d", &major, &minor, &patch) != 3) {
        log_error("mirror-xdg-portal::on_pw_core_info(): failed to parse pipewire version\n");
        backend_fail_async(backend);
        return;
    }

    backend->pw_major = major;
    backend->pw_minor = minor;
    backend->pw_patch = patch;

    screencast_pipewire_create_stream(ctx, backend);
}

static bool screencast_pipewire_check_version(xdg_portal_mirror_backend_t * backend, uint32_t major, uint32_t minor, uint32_t patch) {
    return backend->pw_major == major && backend->pw_minor == minor && backend->pw_patch >= patch;
}

static void on_pw_core_error(void * data, uint32_t id, int seq, int res, const char * msg) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    log_error("mirror-xdg-portal::on_pw_core_error(): got error: id = %d, seq = %d, res = %d, msg = %s\n", id, seq, res, msg);
    backend_fail_async(backend);
}

static void on_pw_stream_process(void * data);
static void on_pw_param_changed(void * data, uint32_t id, const struct spa_pod * param);
static void on_pw_state_changed(void * data, enum pw_stream_state old, enum pw_stream_state new, const char * error);

static const struct pw_stream_events pw_stream_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .state_changed = on_pw_state_changed,
    .param_changed = on_pw_param_changed,
    .process = on_pw_stream_process
};

static void screencast_pipewire_create_stream(ctx_t * ctx, xdg_portal_mirror_backend_t * backend) {
    backend->state = STATE_PW_CREATE_STREAM;

    log_debug(ctx, "mirror-xdg-portal::screencast_pipewire_create_stream(): creating video stream\n");

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

    struct spa_pod_dynamic_builder pod_builder;
    spa_pod_dynamic_builder_init(&pod_builder, NULL, 0, 1);

#define ADD_FORMAT(builder, spa_format, ...) ({ \
        struct spa_pod_builder * b = builder; \
        const uint64_t * modifiers = (uint64_t[]){ __VA_ARGS__ }; \
        size_t num_modifiers = ARRAY_LENGTH(((uint64_t[]){ __VA_ARGS__ })); \
        \
        struct spa_pod_frame format_frame; \
        spa_pod_builder_push_object(b, &format_frame, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat); \
        spa_pod_builder_add(b, \
            SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video), \
            SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), \
            SPA_FORMAT_VIDEO_format, SPA_POD_Id(spa_format), \
            0 \
        ); \
        \
        if (num_modifiers > 0) { \
            spa_pod_builder_prop(b, SPA_FORMAT_VIDEO_modifier, SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE); \
            struct spa_pod_frame modifier_frame; \
            spa_pod_builder_push_choice(b, &modifier_frame, SPA_CHOICE_Enum, 0); \
            spa_pod_builder_long(b, modifiers[0]); \
            for (uint32_t i = 0; i < num_modifiers; i++) { \
                spa_pod_builder_long(b, modifiers[i]); \
            } \
            spa_pod_builder_pop(b, &modifier_frame); \
        } \
        \
        spa_pod_builder_add(b, \
            SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle( \
              &SPA_RECTANGLE(320, 240), /* arbitrary */ \
              &SPA_RECTANGLE(1, 1), /* min */ \
              &SPA_RECTANGLE(8192, 4320) /* max */ \
            ), \
            SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction( \
              &SPA_FRACTION(30 /* ovi->fps_num */, 1 /* ovi->fps_den */), \
              &SPA_FRACTION(0, 1), \
              &SPA_FRACTION(360, 1) \
            ), \
            0 \
        ); \
        spa_pod_builder_pop(&pod_builder.b, &format_frame); \
    })

    // TODO: don't hardcode video format options
    const struct spa_pod * params[] = {
        ADD_FORMAT(&pod_builder.b, SPA_VIDEO_FORMAT_ABGR, 0x0000000000000000, 0x0100000000000001, 0x0100000000000002, 0x0100000000000004, 0x00ffffffffffffff),
        ADD_FORMAT(&pod_builder.b, SPA_VIDEO_FORMAT_ARGB, 0x0000000000000000, 0x0100000000000001, 0x0100000000000002, 0x0100000000000004, 0x00ffffffffffffff),
        ADD_FORMAT(&pod_builder.b, SPA_VIDEO_FORMAT_BGRx, 0x0000000000000000, 0x0100000000000001, 0x0100000000000002, 0x0100000000000004, 0x00ffffffffffffff),
        ADD_FORMAT(&pod_builder.b, SPA_VIDEO_FORMAT_RGBx, 0x0000000000000000, 0x0100000000000001, 0x0100000000000002, 0x0100000000000004, 0x00ffffffffffffff),
        ADD_FORMAT(&pod_builder.b, SPA_VIDEO_FORMAT_ABGR),
        ADD_FORMAT(&pod_builder.b, SPA_VIDEO_FORMAT_ARGB),
        ADD_FORMAT(&pod_builder.b, SPA_VIDEO_FORMAT_BGRx),
        ADD_FORMAT(&pod_builder.b, SPA_VIDEO_FORMAT_RGBx)
    };
    uint32_t num_params = ARRAY_LENGTH(params);

    pw_stream_connect(
        backend->pw_stream, PW_DIRECTION_INPUT, backend->pw_node_id,
        PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS,
        params, num_params
    );

    spa_pod_dynamic_builder_clean(&pod_builder);
}

static void on_pw_stream_process(void * data) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    if (backend->state != STATE_RUNNING) {
        log_debug(ctx, "mirror-xdg-portal::on_pw_stream_process(): called while not running\n");
        return;
    }

    log_debug(ctx, "mirror-xdg-portal::on_pw_stream_process(): new video data\n");

    struct pw_buffer * buffer = NULL;
    struct pw_buffer * next_buffer = NULL;
    while ((next_buffer = pw_stream_dequeue_buffer(backend->pw_stream)) != NULL) {
        if (buffer != NULL) {
            pw_stream_queue_buffer(backend->pw_stream, buffer);
        }

        buffer = next_buffer;
    }

    if (buffer == NULL) {
        log_error("mirror-xdg-portal::on_pw_stream_process(): out of buffers\n");
        backend_fail_async(backend);
        return;
    }

    struct spa_buffer * spa_buffer = buffer->buffer;
    struct spa_meta_header * meta = spa_buffer_find_meta_data(spa_buffer,
        SPA_META_Header, sizeof (struct spa_meta_header)
    );
    if (meta != NULL) {
        log_debug(ctx, "mirror-xdg-portal::on_pw_stream_process(): received meta header\n");

        if ((meta->flags & SPA_META_HEADER_FLAG_CORRUPTED) != 0) {
            log_error("mirror-xdg-portal::on_pw_stream_process(): received corrupt buffer\n");
            pw_stream_queue_buffer(backend->pw_stream, buffer);
            backend_fail_async(backend);
            return;
        }
    }

    if (spa_buffer->datas[0].type == SPA_DATA_DmaBuf) {
        log_debug(ctx, "mirror-xdg-portal::on_pw_stream_process(): received DMA buf\n");

        if (spa_buffer->n_datas > MAX_PLANES) {
            log_error("mirror-xdg-portal::on_pw_stream_process(): max %d planes are supported, got %d planes\n", MAX_PLANES, spa_buffer->n_datas);
            pw_stream_queue_buffer(backend->pw_stream, buffer);
            backend_fail_async(backend);
            return;
        }

        log_debug(ctx, "mirror-xdg-portal::on_pw_stream_process(): w=%d h=%d gl_format=%x drm_format=%c%c%c%c drm_modifier=%016lx\n",
            backend->w, backend->h, backend->gl_format,
            (backend->drm_format >> 0) & 0xFF,
            (backend->drm_format >> 8) & 0xFF,
            (backend->drm_format >> 16) & 0xFF,
            (backend->drm_format >> 24) & 0xFF,
            backend->drm_modifier
        );

        dmabuf_t dmabuf;
        dmabuf.width = backend->w;
        dmabuf.height = backend->h;
        dmabuf.drm_format = backend->drm_format;
        dmabuf.planes = spa_buffer->n_datas;
        dmabuf.fds = malloc(dmabuf.planes * sizeof (int));
        dmabuf.offsets = malloc(dmabuf.planes * sizeof (uint32_t));
        dmabuf.strides = malloc(dmabuf.planes * sizeof (uint32_t));
        dmabuf.modifier = backend->drm_modifier;
        if (dmabuf.fds == NULL || dmabuf.offsets == NULL || dmabuf.strides == NULL) {
            log_error("mirror-xdg-portal::on_pw_stream_process(): failed to allocate dmabuf storage\n");
            pw_stream_queue_buffer(backend->pw_stream, buffer);
            backend_fail_async(backend);
            return;
        }

        bool corrupted = false;
        for (size_t i = 0; i < dmabuf.planes; i++) {
            dmabuf.fds[i] = spa_buffer->datas[i].fd;
            dmabuf.offsets[i] = spa_buffer->datas[i].chunk->offset;
            dmabuf.strides[i] = spa_buffer->datas[i].chunk->stride;
            corrupted |= (bool)(spa_buffer->datas[i].chunk->flags & SPA_CHUNK_FLAG_CORRUPTED);

            log_debug(ctx, "mirror-xdg-portal::on_pw_stream_process(): fd=%d offset=% 10d stride=% 10d type=%d\n",
                dmabuf.fds[i], dmabuf.offsets[i], dmabuf.strides[i], spa_buffer->datas[i].type
            );
        }

        if (corrupted) {
            log_error("mirror-xdg-portal::on_pw_stream_process(): received corrupt dmabuf\n");
            pw_stream_queue_buffer(backend->pw_stream, buffer);
            backend_fail_async(backend);
            free(dmabuf.fds);
            free(dmabuf.offsets);
            free(dmabuf.strides);
            return;
        }

        if (!dmabuf_to_texture(ctx, &dmabuf)) {
            log_error("mirror-xdg-portal::on_pw_stream_process(): failed to import dmabuf\n");
            pw_stream_queue_buffer(backend->pw_stream, buffer);
            backend_fail_async(backend);
            free(dmabuf.fds);
            free(dmabuf.offsets);
            free(dmabuf.strides);
            return;
        }

        free(dmabuf.fds);
        free(dmabuf.offsets);
        free(dmabuf.strides);
        ctx->egl.format = backend->gl_format;
        ctx->egl.texture_region_aware = false;

        if (ctx->mirror.invert_y != false) {
            ctx->mirror.invert_y = false;
            update_uniforms(ctx);
        }

        backend->header.fail_count = 0;
    } else if (spa_buffer->datas[0].type == SPA_DATA_MemPtr) {
        log_debug(ctx, "mirror-xdg-portal::on_pw_stream_process(): received SHM buf\n");

        log_error("mirror-xdg-portal::on_pw_stream_process(): SHM buffers not yet implemented\n");
        backend_fail_async(backend);
    } else {
        log_error("mirror-xdg-portal::on_pw_stream_process(): received unknown buffer type\n");
        backend_fail_async(backend);
    }

    struct spa_meta_region * region = spa_buffer_find_meta_data(spa_buffer,
        SPA_META_VideoCrop, sizeof (struct spa_meta_region)
    );
    if (region != NULL && spa_meta_region_is_valid(region)) {
        log_debug(ctx, "mirror-xdg-portal::on_pw_stream_process(): received meta region\n");
        // TODO: handle region
    }

    struct spa_meta_videotransform * transform = spa_buffer_find_meta_data(spa_buffer,
        SPA_META_VideoTransform, sizeof (struct spa_meta_videotransform)
    );
    if (transform != NULL) {
        log_debug(ctx, "mirror-xdg-portal::on_pw_stream_process(): received meta transform\n");
        // TODO: handle transform
    }

    pw_stream_queue_buffer(backend->pw_stream, buffer);
}

static void on_pw_param_changed(void * data, uint32_t id, const struct spa_pod * param) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    if (backend->state != STATE_RUNNING && backend->state != STATE_PW_CREATE_STREAM) {
        log_debug(ctx, "mirror-xdg-portal::on_pw_param_changed(): called while not running\n");
        return;
    }

    if (id == SPA_PARAM_Format) {
        log_debug(ctx, "mirror-xdg-portal::on_pw_param_changed(): format changed\n");

        uint32_t media_type;
        uint32_t media_subtype;
        if (spa_format_parse(param, &media_type, &media_subtype) < 0) {
            log_error("mirror-xdg-portal::on_pw_param_changed(): failed to parse SPA format\n");
            backend_fail_async(backend);
            return;
        }

        if (media_type != SPA_MEDIA_TYPE_video && media_subtype != SPA_MEDIA_SUBTYPE_raw) {
            log_error("mirror-xdg-portal::on_pw_param_changed(): unsupported media type '%d.%d'\n", media_type, media_subtype);
            backend_fail_async(backend);
            return;
        }

        struct spa_video_info_raw info_raw;
        if (spa_format_video_raw_parse(param, &info_raw) < 0) {
            log_error("mirror-xdg-portal::on_pw_param_changed(): failed to parse SPA raw format\n");
            backend_fail_async(backend);
            return;
        }
        backend->w = info_raw.size.width;
        backend->h = info_raw.size.height;
        backend->drm_modifier = info_raw.modifier;

        const spa_drm_gl_format_t * format = spa_drm_gl_format_from_spa(info_raw.format);
        if (format == NULL) {
            log_error("mirror-xdg-portal::on_pw_param_changed(): unsupported SPA format '%d'\n", info_raw.format);
            backend_fail_async(backend);
            return;
        }
        backend->gl_format = format->gl_format;
        backend->drm_format = format->drm_format;

        log_debug(ctx, "mirror-xdg-portal::on_pw_param_changed(): w=%d h=%d gl_format=%x drm_format=%c%c%c%c drm_modifier=%016lx\n",
            backend->w, backend->h, backend->gl_format,
            (backend->drm_format >> 0) & 0xFF,
            (backend->drm_format >> 8) & 0xFF,
            (backend->drm_format >> 16) & 0xFF,
            (backend->drm_format >> 24) & 0xFF,
            backend->drm_modifier
        );

        log_debug(ctx, "mirror-xdg-portal::on_pw_param_changed(): spa_format=%d framerate=%d/%d\n",
            info_raw.format, info_raw.framerate.num, info_raw.framerate.denom
        );

        uint32_t supported_buffer_types = (1 << SPA_DATA_MemPtr);
        bool has_modifier = spa_pod_find_prop(param, NULL, SPA_FORMAT_VIDEO_modifier) != NULL;
        if (has_modifier || screencast_pipewire_check_version(backend, 0, 3, 24)) {
            supported_buffer_types |= (1 << SPA_DATA_DmaBuf);
        }

        struct spa_pod_dynamic_builder pod_builder;
        spa_pod_dynamic_builder_init(&pod_builder, NULL, 0, 1);

        const struct spa_pod * params[] = {
            // buffer options
            spa_pod_builder_add_object(&pod_builder.b,
                SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
                SPA_PARAM_BUFFERS_dataType, SPA_POD_Int(supported_buffer_types)
            ),
            // meta header
            spa_pod_builder_add_object(&pod_builder.b,
                SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Header),
                SPA_PARAM_META_size, SPA_POD_Int(sizeof (struct spa_meta_header))
            ),
            // region
            spa_pod_builder_add_object(&pod_builder.b,
                SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoCrop),
                SPA_PARAM_META_size, SPA_POD_Int(sizeof (struct spa_meta_region))
            ),
            // transform
            spa_pod_builder_add_object(&pod_builder.b,
                SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoTransform),
                SPA_PARAM_META_size, SPA_POD_Int(sizeof (struct spa_meta_videotransform))
            ),
        };
        uint32_t num_params = ARRAY_LENGTH(params);

        pw_stream_update_params(backend->pw_stream, params, num_params);

        spa_pod_dynamic_builder_clean(&pod_builder);

        // TODO: remember that negotiation already happened
        backend->state = STATE_RUNNING;
    } else {
        log_debug(ctx, "mirror-xdg-portal::on_pw_param_changed(): unknown param id = %d\n", id);
    }

    (void)backend;
}

static void on_pw_state_changed(void * data, enum pw_stream_state old, enum pw_stream_state new, const char * error) {
    ctx_t * ctx = (ctx_t *)data;
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    const char * old_s = pw_stream_state_as_string(old);
    const char * new_s = pw_stream_state_as_string(new);
    log_debug(ctx, "mirror-xdg-portal::on_pw_state_changed(): state = %s -> %s, error = %s\n", old_s, new_s, error);

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

    backend->state = STATE_BROKEN;

    log_debug(ctx, "mirror-xdg-portal::do_cleanup(): destroying mirror-xdg-portal objects\n");

    // deregister event handlers
    if (backend->bus != NULL) event_remove_fd(ctx, &backend->dbus_event_handler);
    if (backend->pw_loop != NULL) event_remove_fd(ctx, &backend->pw_event_handler);

    // release pipewire resources
    if (backend->pw_stream != NULL) pw_stream_destroy(backend->pw_stream);
    if (backend->pw_core != NULL) pw_core_disconnect(backend->pw_core);
    if (backend->pw_context != NULL) pw_context_destroy(backend->pw_context);
    if (backend->pw_loop != NULL) pw_loop_destroy(backend->pw_loop);
    if (backend->pw_fd != -1) close(backend->pw_fd);

    // close portal session
    if (backend->session_open) {
        sd_bus_call_method_async(
            backend->bus, NULL,
            "org.freedesktop.portal.Desktop",
            backend->session_handle,
            "org.freedesktop.portal.Session", "Close",
            NULL, NULL,
            ""
        );
    }

    // release sd-bus resources
    if (backend->session_slot != NULL) sd_bus_slot_unref(backend->session_slot);
    if (backend->call_slot != NULL) sd_bus_slot_unref(backend->call_slot);
    if (backend->request_handle != NULL) free(backend->request_handle);
    if (backend->session_handle != NULL) free(backend->session_handle);
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

    int epoll_events = 0;
    if (events & POLLIN)        epoll_events |= EPOLLIN;
    if (events & POLLRDNORM)    epoll_events |= EPOLLRDNORM;
    if (events & POLLRDBAND)    epoll_events |= EPOLLRDBAND;
    if (events & POLLPRI)       epoll_events |= EPOLLPRI;
    if (events & POLLOUT)       epoll_events |= EPOLLOUT;
    if (events & POLLWRNORM)    epoll_events |= EPOLLWRNORM;
    if (events & POLLWRBAND)    epoll_events |= EPOLLWRBAND;
    if (events & POLLERR)       epoll_events |= EPOLLERR;
    if (events & POLLHUP)       epoll_events |= EPOLLHUP;
    backend->dbus_event_handler.events = epoll_events;

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
    while ((ret = sd_bus_process(backend->bus, NULL)) > 0);

    if (ret < 0) {
        log_error("mirror-xdg-portal::on_loop_pw_event(): failed to process dbus events\n");
        backend_fail(ctx);
    }
}

static void on_loop_dbus_each(ctx_t * ctx) {
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    event_handler_t old_handler = backend->dbus_event_handler;
    if (!update_bus_events(backend)) {
        log_error("mirror-xdg-portal::on_loop_dbus_each(): failed to update dbus pollfd\n");
        backend_fail(ctx);
    }

    if (old_handler.fd != backend->dbus_event_handler.fd) {
        event_remove_fd(ctx, &old_handler);
        event_add_fd(ctx, &backend->dbus_event_handler);
    } else if (old_handler.events != backend->dbus_event_handler.events) {
        event_change_fd(ctx, &backend->dbus_event_handler);
    }
}

static void on_loop_pw_event(ctx_t * ctx) {
    xdg_portal_mirror_backend_t * backend = (xdg_portal_mirror_backend_t *)ctx->mirror.backend;

    pw_loop_enter(backend->pw_loop);
    int ret = pw_loop_iterate(backend->pw_loop, 0);
    pw_loop_leave(backend->pw_loop);

    if (ret < 0) {
        log_error("mirror-xdg-portal::on_loop_pw_event(): failed to process pipewire events\n");
        backend_fail(ctx);
    }
}

// --- init_mirror_xdg_portal ---

void init_mirror_xdg_portal(ctx_t * ctx) {
    // check for required protocols
    if (ctx->wl.xdg_exporter == NULL) {
        log_warn("mirror-xdg-portal::init(): missing xdg_foreign protocol\n");
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
    backend->gl_format = 0;
    backend->drm_format = 0;
    backend->drm_modifier = 0;

    // sd-bus state
    backend->screencast_properties = (screencast_properties_t) {
        .source_types = 0,
        .cursor_modes = 0,
        .version = 0
    };
    backend->request_handle = NULL;
    backend->session_handle = NULL;
    backend->session_open = false;

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
    backend->pw_major = 0;
    backend->pw_minor = 0;
    backend->pw_patch = 0;

    backend->pw_loop = NULL;
    backend->pw_context = NULL;
    backend->pw_core = NULL;
    backend->pw_stream = NULL;
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
