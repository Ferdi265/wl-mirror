#ifndef WL_MIRROR_CONTEXT_H_
#define WL_MIRROR_CONTEXT_H_

#include <stdint.h>
#include <stdbool.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "xdg-shell.h"
#include "xdg-output-unstable-v1.h"
#include "linux-dmabuf-unstable-v1.h"
#include "wlr-export-dmabuf-unstable-v1.h"

typedef struct ctx ctx_t;
typedef struct output_list_node output_list_node_t;

struct output_list_node {
    output_list_node_t * next;
    ctx_t * ctx;
    char * name;
    struct wl_output * output;
    struct zxdg_output_v1 * xdg_output;
    uint32_t output_id;
    int32_t scale;
};

typedef struct {
    struct wl_display * display;
    struct wl_registry * registry;

    // registry objects
    struct wl_compositor * compositor;
    struct xdg_wm_base * wm_base;
    struct zxdg_output_manager_v1 * output_manager;
    struct zwlr_export_dmabuf_manager_v1 * dmabuf_manager;
    // registry ids
    uint32_t compositor_id;
    uint32_t wm_base_id;
    uint32_t output_manager_id;
    uint32_t dmabuf_manager_id;

    // output list
    output_list_node_t * outputs;

    // surface objects
    struct wl_surface * surface;
    struct xdg_surface * xdg_surface;
    struct xdg_toplevel * xdg_toplevel;

    // buffer size
    output_list_node_t * current_output;
    uint32_t width;
    uint32_t height;
    int32_t scale;

    // state flags
    uint32_t last_surface_serial;
    bool xdg_surface_configured;
    bool xdg_toplevel_configured;
    bool configured;
    bool closing;
} ctx_wl_t;

typedef struct {
    EGLDisplay display;
    EGLContext context;
    EGLConfig config;
    EGLSurface surface;
    struct wl_egl_window * window;

    // extension functions
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

    // texture size
    uint32_t width;
    uint32_t height;

    // gl objects
    GLuint vbo;
    GLuint texture;
    GLuint shader_program;
    GLint invert_y_uniform;
    bool texture_initialized;
    bool invert_y;
} ctx_egl_t;

typedef struct {
    int32_t fd;
    uint32_t size;
    uint32_t offset;
    uint32_t stride;
    uint32_t plane_index;
} ctx_mirror_object_t;

typedef enum {
    STATE_WAIT_FRAME,
    STATE_WAIT_OBJECTS,
    STATE_WAIT_READY,
    STATE_READY,
    STATE_CANCELED
} ctx_mirror_state_t;

typedef struct {
    output_list_node_t * current;
    struct wl_callback * frame_callback;
    struct zwlr_export_dmabuf_frame_v1 * frame;

    // gl data
    EGLImage frame_image;

    // frame data
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    uint32_t buffer_flags;
    uint32_t frame_flags;
    uint32_t format;
    uint32_t modifier_low;
    uint32_t modifier_high;
    uint32_t num_objects;

    // object data
    ctx_mirror_object_t objects[4];

    // state flags
    ctx_mirror_state_t state;
    uint32_t processed_objects;
} ctx_mirror_t;

struct ctx {
    ctx_wl_t * wl;
    ctx_egl_t * egl;
    ctx_mirror_t * mirror;
};

void init_wl(ctx_t * ctx);
void init_egl(ctx_t * ctx);
void init_mirror(ctx_t * ctx, char * output);

void draw_texture_egl(ctx_t * ctx);
void resize_viewport_egl(ctx_t * ctx);
void resize_window_egl(ctx_t * ctx);

void output_removed_handler_mirror(ctx_t * ctx, output_list_node_t * node);

void exit_fail(ctx_t * ctx);
void cleanup(ctx_t * ctx);
void cleanup_wl(ctx_t * ctx);
void cleanup_egl(ctx_t * ctx);
void cleanup_mirror(ctx_t * ctx);

#endif
