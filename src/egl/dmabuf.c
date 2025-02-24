#include <wlm/context.h>
#include <wlm/egl/dmabuf.h>
#include <wlm/egl/formats.h>
#include <wlm/util.h>
#include <stdlib.h>

static const EGLAttrib fd_attribs[] = {
    EGL_DMA_BUF_PLANE0_FD_EXT,
    EGL_DMA_BUF_PLANE1_FD_EXT,
    EGL_DMA_BUF_PLANE2_FD_EXT,
    EGL_DMA_BUF_PLANE3_FD_EXT
};
_Static_assert(ARRAY_LENGTH(fd_attribs) == MAX_PLANES, "fd_attribs has incorrect length");

static const EGLAttrib offset_attribs[] = {
    EGL_DMA_BUF_PLANE0_OFFSET_EXT,
    EGL_DMA_BUF_PLANE1_OFFSET_EXT,
    EGL_DMA_BUF_PLANE2_OFFSET_EXT,
    EGL_DMA_BUF_PLANE3_OFFSET_EXT
};
_Static_assert(ARRAY_LENGTH(offset_attribs) == MAX_PLANES, "offset_attribs has incorrect length");

static const EGLAttrib stride_attribs[] = {
    EGL_DMA_BUF_PLANE0_PITCH_EXT,
    EGL_DMA_BUF_PLANE1_PITCH_EXT,
    EGL_DMA_BUF_PLANE2_PITCH_EXT,
    EGL_DMA_BUF_PLANE3_PITCH_EXT
};
_Static_assert(ARRAY_LENGTH(stride_attribs) == MAX_PLANES, "stride_attribs has incorrect length");

static const EGLAttrib modifier_low_attribs[] = {
    EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
    EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
    EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT,
    EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT
};
_Static_assert(ARRAY_LENGTH(modifier_low_attribs) == MAX_PLANES, "modifier_low_attribs has incorrect length");

static const EGLAttrib modifier_high_attribs[] = {
    EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
    EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
    EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT,
    EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT
};
_Static_assert(ARRAY_LENGTH(modifier_high_attribs) == MAX_PLANES, "modifier_high_attribs has incorrect length");

bool wlm_egl_dmabuf_import(ctx_t * ctx, dmabuf_t * dmabuf, const wlm_egl_format_t * format, bool invert_y, bool region_aware) {
    if (dmabuf->planes > MAX_PLANES) {
        wlm_log_error("egl::dmabuf::import(): too many planes, got %zd, can support at most %d\n", dmabuf->planes, MAX_PLANES);
        return false;
    }

    int i = 0;
    EGLAttrib * image_attribs = calloc((6 + 10 * dmabuf->planes + 1), sizeof (EGLAttrib));
    if (image_attribs == NULL) {
        wlm_log_error("egl::dmabuf::import(): failed to allocate EGL image attribs\n");
        return false;
    }

    image_attribs[i++] = EGL_WIDTH;
    image_attribs[i++] = dmabuf->width;
    image_attribs[i++] = EGL_HEIGHT;
    image_attribs[i++] = dmabuf->height;
    image_attribs[i++] = EGL_LINUX_DRM_FOURCC_EXT;
    image_attribs[i++] = dmabuf->drm_format;

    for (size_t j = 0; j < dmabuf->planes; j++) {
        image_attribs[i++] = fd_attribs[j];
        image_attribs[i++] = dmabuf->fds[j];
        image_attribs[i++] = offset_attribs[j];
        image_attribs[i++] = dmabuf->offsets[j];
        image_attribs[i++] = stride_attribs[j];
        image_attribs[i++] = dmabuf->strides[j];
        image_attribs[i++] = modifier_low_attribs[j];
        image_attribs[i++] = (uint32_t)dmabuf->modifier;
        image_attribs[i++] = modifier_high_attribs[j];
        image_attribs[i++] = (uint32_t)(dmabuf->modifier >> 32);
    }

    image_attribs[i++] = EGL_NONE;

    // create EGLImage from dmabuf with attribute array
    EGLImage frame_image = eglCreateImage(ctx->egl.display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, image_attribs);
    free(image_attribs);

    if (frame_image == EGL_NO_IMAGE) {
        wlm_log_error("egl::dmabuf::import(): failed to create EGL image from DMA-BUF: error = %x\n", eglGetError());
        return false;
    }

    // convert EGLImage to GL texture
    glBindTexture(GL_TEXTURE_2D, ctx->egl.texture);
    ctx->egl.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, frame_image);

    // destroy temporary image
    eglDestroyImage(ctx->egl.display, frame_image);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        wlm_log_error("egl::dmabuf::import(): failed to import DMA-BUF: GL error %s (0x%x)\n", glGetString(error), error);
        return false;
    }

    ctx->egl.format = format != NULL ? format->gl_format : GL_RGB8_OES; // TODO: remove this fallback
    ctx->egl.texture_initialized = true;
    ctx->egl.texture_region_aware = region_aware;

    // set buffer flags
    if (ctx->mirror.invert_y != invert_y) {
        ctx->mirror.invert_y = invert_y;
        wlm_egl_update_uniforms(ctx);
    }

    // set texture size and aspect ratio only if changed
    if (dmabuf->width != ctx->egl.width || dmabuf->height != ctx->egl.height) {
        ctx->egl.width = dmabuf->width;
        ctx->egl.height = dmabuf->height;
        wlm_egl_resize_viewport(ctx);
    }

    return true;
}
