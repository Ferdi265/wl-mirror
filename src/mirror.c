#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "context.h"

// --- dmabuf_frame event handlers ---

static void dmabuf_frame_event_frame(
    void * data, struct zwlr_export_dmabuf_frame_v1 * frame,
    uint32_t width, uint32_t height, uint32_t x, uint32_t y,
    uint32_t buffer_flags, uint32_t frame_flags, uint32_t format,
    uint32_t mod_high, uint32_t mod_low, uint32_t num_objects
) {
    ctx_t * ctx = (ctx_t *)data;
    printf("[info] dmabuf_frame: received %dx%d frame with %d objects\n", width, height, num_objects);
    if (ctx->mirror->state != STATE_WAIT_FRAME) {
        printf("[error] dmabuf_frame: got frame while in state %d\n", ctx->mirror->state);
        exit_fail(ctx);
    } else if (num_objects > 4) {
        printf("[error] dmabuf_frame: got frame with more than 4 objects\n");
        exit_fail(ctx);
    }

    ctx->mirror->width = width;
    ctx->mirror->height = height;
    ctx->mirror->x = x;
    ctx->mirror->y = y;
    ctx->mirror->buffer_flags = buffer_flags;
    ctx->mirror->frame_flags = frame_flags;
    ctx->mirror->format = format;
    ctx->mirror->modifier = ((uint64_t)mod_high << 32) | mod_low;
    ctx->mirror->num_objects = num_objects;

    ctx->mirror->state = STATE_WAIT_OBJECTS;
    ctx->mirror->processed_objects = 0;
}

static void dmabuf_frame_event_object(
    void * data, struct zwlr_export_dmabuf_frame_v1 * frame,
    uint32_t index, int32_t fd, uint32_t size,
    uint32_t offset, uint32_t stride, uint32_t plane_index
) {
    ctx_t * ctx = (ctx_t *)data;
    printf("[info] dmabuf_frame: received %d byte object with plane_index %d\n", size, plane_index);
    if (ctx->mirror->state != STATE_WAIT_OBJECTS) {
        printf("[error] dmabuf_frame: got object while in state %d\n", ctx->mirror->state);
        exit_fail(ctx);
    } else if (index >= ctx->mirror->num_objects) {
        printf("[error] dmabuf_frame: got object with out-of-bounds index %d\n", index);
        exit_fail(ctx);
    }

    ctx->mirror->objects[index].fd = fd;
    ctx->mirror->objects[index].size = size;
    ctx->mirror->objects[index].offset = offset;
    ctx->mirror->objects[index].stride = stride;
    ctx->mirror->objects[index].plane_index = plane_index;

    ctx->mirror->processed_objects++;
    if (ctx->mirror->processed_objects == ctx->mirror->num_objects) {
        ctx->mirror->state = STATE_WAIT_READY;
    }
}

static void dmabuf_frame_event_ready(
    void * data, struct zwlr_export_dmabuf_frame_v1 * frame,
    uint32_t sec_hi, uint32_t sec_lo, uint32_t nsec
) {
    ctx_t * ctx = (ctx_t *)data;
    printf("[info] dmabuf_frame: frame is ready\n");
    if (ctx->mirror->state != STATE_WAIT_READY) {
        printf("[error] dmabuf_frame: got ready while in state %d\n", ctx->mirror->state);
        exit_fail(ctx);
    }

    printf("[info] dmabuf_frame: creating texture from dmabuf\n");
    // TODO: create texture

    for (int i = 0; i < ctx->mirror->num_objects; i++) {
        close(ctx->mirror->objects[i].fd);
    }

    zwlr_export_dmabuf_frame_v1_destroy(ctx->mirror->frame);
    ctx->mirror->frame = NULL;
    ctx->mirror->state = STATE_READY;

    (void)sec_hi;
    (void)sec_lo;
    (void)nsec;
}

static void dmabuf_frame_event_cancel(
    void * data, struct zwlr_export_dmabuf_frame_v1 * frame,
    enum zwlr_export_dmabuf_frame_v1_cancel_reason reason
) {
    ctx_t * ctx = (ctx_t *)data;
    printf("[info] dmabuf_frame: frame was canceled\n");

    zwlr_export_dmabuf_frame_v1_destroy(ctx->mirror->frame);
    ctx->mirror->frame = NULL;
    ctx->mirror->state = STATE_CANCELED;

    switch (reason) {
        case ZWLR_EXPORT_DMABUF_FRAME_V1_CANCEL_REASON_PERMANENT:
            printf("[error] dmabuf_frame: permanent cancellation\n");
            exit_fail(ctx);
            break;

        case ZWLR_EXPORT_DMABUF_FRAME_V1_CANCEL_REASON_TEMPORARY:
            printf("[info] dmabuf_frame: temporary cancellation\n");
            break;

        case ZWLR_EXPORT_DMABUF_FRAME_V1_CANCEL_REASON_RESIZING:
            printf("[info] dmabuf_frame: cancellation due to output resize\n");
            break;

        default:
            printf("[error] dmabuf_frame: unknown cancellation reason %d\n", reason);
            exit_fail(ctx);
            break;
    }

    printf("[info] dmabuf_frame: closing files\n");
    for (int i = 0; i < ctx->mirror->num_objects; i++) {
        if (ctx->mirror->objects[i].fd != -1) close(ctx->mirror->objects[i].fd);
        ctx->mirror->objects[i].fd = -1;
    }
}

static const struct zwlr_export_dmabuf_frame_v1_listener dmabuf_frame_listener = {
    .frame = dmabuf_frame_event_frame,
    .object = dmabuf_frame_event_object,
    .ready = dmabuf_frame_event_ready,
    .cancel = dmabuf_frame_event_cancel
};

// --- frame_callback event handlers ---

static const struct wl_callback_listener frame_callback_listener;

static void frame_callback_event_done(
    void * data, struct wl_callback * frame_callback, uint32_t msec
) {
    ctx_t * ctx = (ctx_t *)data;

    wl_callback_destroy(ctx->mirror->frame_callback);
    ctx->mirror->frame_callback = NULL;

    printf("[info] frame_callback: requesting next callback\n");
    ctx->mirror->frame_callback = wl_surface_frame(ctx->wl->surface);
    wl_callback_add_listener(ctx->mirror->frame_callback, &frame_callback_listener, (void *)ctx);

    printf("[info] frame_callback: rendering frame\n");

    glClearColor(0.0, 0.0, 0.0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();

    // TODO: render with image / texture

    printf("[info] frame_callback: swapping buffers\n");
    eglSwapInterval(ctx->egl->display, 0);
    if (eglSwapBuffers(ctx->egl->display, ctx->egl->surface) != EGL_TRUE) {
        printf("[error] frame_callback: failed to swap buffers\n");
        exit_fail(ctx);
    }

    printf("[info] frame_callback: committing surface\n");
    wl_surface_commit(ctx->wl->surface);

    if (ctx->mirror->state != STATE_WAIT_FRAME) {
        printf("[info] frame_callback: clearing dmabuf_frame state\n");
        ctx->mirror->width = 0;
        ctx->mirror->height = 0;
        ctx->mirror->x = 0;
        ctx->mirror->y = 0;
        ctx->mirror->buffer_flags = 0;
        ctx->mirror->frame_flags = 0;
        ctx->mirror->modifier = 0;
        ctx->mirror->format = 0;
        ctx->mirror->num_objects = 0;

        ctx_mirror_object_t empty_obj = {
            .fd = -1,
            .size = 0,
            .offset = 0,
            .stride = 0,
            .plane_index = 0
        };
        ctx->mirror->objects[0] = empty_obj;
        ctx->mirror->objects[1] = empty_obj;
        ctx->mirror->objects[2] = empty_obj;
        ctx->mirror->objects[3] = empty_obj;

        ctx->mirror->state = STATE_WAIT_FRAME;
        ctx->mirror->processed_objects = 0;

        printf("[info] frame_callback: creating wlr_dmabuf_export_frame\n");
        ctx->mirror->frame = zwlr_export_dmabuf_manager_v1_capture_output(
            ctx->wl->dmabuf_manager, true, ctx->mirror->current->output
        );
        if (ctx->mirror->frame == NULL) {
            printf("[error] frame_callback: failed to create wlr_dmabuf_export_frame\n");
            exit_fail(ctx);
        }

        printf("[info] frame_callback: adding dmabuf_frame event listener\n");
        zwlr_export_dmabuf_frame_v1_add_listener(ctx->mirror->frame, &dmabuf_frame_listener, (void *)ctx);
    }
}

static const struct wl_callback_listener frame_callback_listener = {
    .done = frame_callback_event_done
};

// --- init_mirror ---

void init_mirror(ctx_t * ctx, char * output) {
    printf("[info] init_mirror: allocating context structure\n");
    ctx->mirror = malloc(sizeof (ctx_mirror_t));
    if (ctx->mirror == NULL) {
        printf("[error] init_wl: failed to allocate context structure\n");
        exit_fail(ctx);
    }

    ctx->mirror->current = NULL;
    ctx->mirror->frame_callback = NULL;
    ctx->mirror->frame = NULL;

    ctx->mirror->width = 0;
    ctx->mirror->height = 0;
    ctx->mirror->x = 0;
    ctx->mirror->y = 0;
    ctx->mirror->buffer_flags = 0;
    ctx->mirror->frame_flags = 0;
    ctx->mirror->modifier = 0;
    ctx->mirror->format = 0;
    ctx->mirror->num_objects = 0;

    ctx_mirror_object_t empty_obj = {
        .fd = -1,
        .size = 0,
        .offset = 0,
        .stride = 0,
        .plane_index = 0
    };
    ctx->mirror->objects[0] = empty_obj;
    ctx->mirror->objects[1] = empty_obj;
    ctx->mirror->objects[2] = empty_obj;
    ctx->mirror->objects[3] = empty_obj;

    ctx->mirror->state = STATE_CANCELED;
    ctx->mirror->processed_objects = 0;

    printf("[info] init_mirror: searching for output\n");
    output_list_node_t * cur = ctx->wl->outputs;
    while (cur != NULL) {
        if (cur->name != NULL && strcmp(cur->name, output) == 0) {
            ctx->mirror->current = cur;
            break;
        }

        cur = cur->next;
    }

    if (ctx->mirror->current == NULL) {
        printf("[error] init_mirror: output %s not found\n", output);
        exit_fail(ctx);
    } else {
        printf("[info] init_mirror: found output with name %s\n", output);
    }

    printf("[info] init_mirror: formatting window title\n");
    char * title = NULL;
    asprintf(&title, "Wayland Output Mirror for %s", output);
    if (title == NULL) {
        printf("[error] init_mirror: failed to format window title\n");
        exit_fail(ctx);
    }

    printf("[info] init_mirror: setting window title\n");
    xdg_toplevel_set_title(ctx->wl->xdg_toplevel, title);
    free(title);

    printf("[info] init_mirror: requesting render callback\n");
    ctx->mirror->frame_callback = wl_surface_frame(ctx->wl->surface);
    wl_callback_add_listener(ctx->mirror->frame_callback, &frame_callback_listener, (void *)ctx);
}

// --- output_removed_handler_mirror ---

void output_removed_handler_mirror(ctx_t * ctx, output_list_node_t * node) {
    if (ctx->mirror == NULL) return;
    if (ctx->mirror->current == NULL) return;
    if (ctx->mirror->current != node) return;

    printf("[error] output_removed_handler_mirror: output disappeared, closing\n");
    exit_fail(ctx);
}

// --- cleanup_mirror ---

void cleanup_mirror(ctx_t *ctx) {
    if (ctx->mirror == NULL) return;

    printf("[info] cleanup_mirror: destroying mirror objects\n");
    if (ctx->mirror->frame_callback != NULL) wl_callback_destroy(ctx->mirror->frame_callback);
    if (ctx->mirror->frame != NULL) zwlr_export_dmabuf_frame_v1_destroy(ctx->mirror->frame);

    for (int i = 0; i < 4; i++) {
        if (ctx->mirror->objects[i].fd != -1) close(ctx->mirror->objects[i].fd);
        ctx->mirror->objects[i].fd = -1;
    }

    free(ctx->mirror);
    ctx->mirror = NULL;
}
