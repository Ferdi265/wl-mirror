#include <wlm/context.h>
#include <wlm/egl/shm.h>
#include <wlm/egl/formats.h>

bool wlm_egl_shm_import(ctx_t * ctx, void * shm_addr, const wlm_egl_format_t * format, uint32_t width, uint32_t height, uint32_t stride, bool invert_y) {
    // store frame data into texture
    glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / (format->bpp / 8));
    glTexImage2D(GL_TEXTURE_2D,
        0, format->gl_format, width, height,
        0, format->gl_format, format->gl_type, shm_addr
    );
    glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);
    ctx->egl.format = format->gl_format;
    ctx->egl.texture_region_aware = true;
    ctx->egl.texture_initialized = true;

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        wlm_log_error("egl::shm::import(): failed to import shm buffer: GL error %s (%x)\n", glGetString(error), error);
        return false;
    }

    // set buffer flags
    if (ctx->mirror.invert_y != invert_y) {
        ctx->mirror.invert_y = invert_y;
        wlm_egl_update_uniforms(ctx);
    }

    // set texture size and aspect ratio only if changed
    if (width != ctx->egl.width || height != ctx->egl.height) {
        ctx->egl.width = width;
        ctx->egl.height = height;
        wlm_egl_resize_viewport(ctx);
    }

    return true;
}
