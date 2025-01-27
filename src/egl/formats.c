#include <wlm/egl/formats.h>
#include <GLES2/gl2ext.h>

// TODO: add missing wl_shm, drm, and spa formats
// TODO: verify these format assignments are correct
// TODO: check gl formats
static const wlm_egl_format_t formats[] = {
    {
        .wl_shm_format = WL_SHM_FORMAT_ARGB8888,
        .drm_format = DRM_FORMAT(ARGB8888),
        .spa_format = SPA_FORMAT(BGRA),
        .bpp = 32,
        .gl_format = GL_BGRA_EXT,
        .gl_type = GL_UNSIGNED_BYTE,
    },
    {
        .wl_shm_format = WL_SHM_FORMAT_XRGB8888,
        .drm_format = DRM_FORMAT(XRGB8888),
        .spa_format = SPA_FORMAT(BGRx),
        .bpp = 32,
        .gl_format = GL_BGRA_EXT,
        .gl_type = GL_UNSIGNED_BYTE,
    },
    {
        .wl_shm_format = WL_SHM_FORMAT_ABGR8888,
        .drm_format = DRM_FORMAT(ABGR8888),
        .spa_format = SPA_FORMAT(RGBA),
        .bpp = 32,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_BYTE,
    },
    {
        .wl_shm_format = WL_SHM_FORMAT_XBGR8888,
        .drm_format = DRM_FORMAT(XBGR8888),
        .spa_format = SPA_FORMAT(RGBx),
        .bpp = 32,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_BYTE,
    },
    {
        .wl_shm_format = WL_SHM_FORMAT_RGB888,
        .drm_format = DRM_FORMAT(RGB888),
        .spa_format = SPA_FORMAT(BGR),
        .bpp = 24,
        .gl_format = GL_BGR_EXT,
        .gl_type = GL_UNSIGNED_BYTE,
    },
    {
        .wl_shm_format = WL_SHM_FORMAT_BGR888,
        .drm_format = DRM_FORMAT(BGR888),
        .spa_format = SPA_FORMAT(RGB),
        .bpp = 24,
        .gl_format = GL_RGB,
        .gl_type = GL_UNSIGNED_BYTE,
    },
    {
        .wl_shm_format = WL_SHM_FORMAT_RGBX4444,
        .drm_format = DRM_FORMAT(RGBX4444),
        .spa_format = SPA_FORMAT(UNKNOWN), // TODO
        .bpp = 16,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_SHORT_4_4_4_4,
    },
    {
        .wl_shm_format = WL_SHM_FORMAT_RGBA4444,
        .drm_format = DRM_FORMAT(RGBA4444),
        .spa_format = SPA_FORMAT(UNKNOWN), // TODO
        .bpp = 16,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_SHORT_4_4_4_4,
    },
    {
        .wl_shm_format = WL_SHM_FORMAT_RGBX5551,
        .drm_format = DRM_FORMAT(RGBX5551),
        .spa_format = SPA_FORMAT(BGR15), // TODO: correct?
        .bpp = 16,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_SHORT_5_5_5_1,
    },
    {
        .wl_shm_format = WL_SHM_FORMAT_RGBA5551,
        .drm_format = DRM_FORMAT(RGBA5551),
        .spa_format = SPA_FORMAT(UNKNOWN), // TODO
        .bpp = 16,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_SHORT_5_5_5_1,
    },
    {
        .wl_shm_format = WL_SHM_FORMAT_RGB565,
        .drm_format = DRM_FORMAT(RGB565),
        .spa_format = SPA_FORMAT(BGR16), // TODO: correct?
        .bpp = 16,
        .gl_format = GL_RGB,
        .gl_type = GL_UNSIGNED_SHORT_5_6_5,
    },
    {
        .wl_shm_format = WL_SHM_FORMAT_XBGR2101010,
        .drm_format = DRM_FORMAT(XBGR2101010),
        .spa_format = SPA_FORMAT(RGBx_102LE), // TODO: correct?
        .bpp = 32,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_INT_2_10_10_10_REV_EXT,
    },
    {
        .wl_shm_format = WL_SHM_FORMAT_ABGR2101010,
        .drm_format = DRM_FORMAT(ABGR2101010),
        .spa_format = SPA_FORMAT(RGBA_102LE), // TODO
        .bpp = 32,
        .gl_format = GL_RGBA,
        .gl_type = GL_UNSIGNED_INT_2_10_10_10_REV_EXT,
    },
    {
        .wl_shm_format = WL_SHM_FORMAT_XBGR16161616F,
        .drm_format = DRM_FORMAT(XBGR16161616F),
        .spa_format = SPA_FORMAT(UNKNOWN), // TODO
        .bpp = 64,
        .gl_format = GL_RGBA,
        .gl_type = GL_HALF_FLOAT_OES,
    },
    {
        .wl_shm_format = WL_SHM_FORMAT_ABGR16161616F,
        .drm_format = DRM_FORMAT(ABGR16161616F),
        .spa_format = SPA_FORMAT(RGBA_F16), // TODO: correct?
        .bpp = 64,
        .gl_format = GL_RGBA,
        .gl_type = GL_HALF_FLOAT_OES,
    },
    {
        .wl_shm_format = -1U,
        .bpp = -1U,
        .gl_format = -1,
        .gl_type = -1
    }
};

const wlm_egl_format_t * wlm_egl_formats_find_shm(enum wl_shm_format shm_format) {
    const wlm_egl_format_t * format = formats;
    while (format->bpp != -1U) {
        if (format->wl_shm_format == shm_format) {
            return format;
        }

        format++;
    }

    return NULL;
}

const wlm_egl_format_t * wlm_egl_formats_find_drm(uint32_t drm_format) {
    const wlm_egl_format_t * format = formats;
    while (format->bpp != -1U) {
        if (format->drm_format == drm_format) {
            return format;
        }

        format++;
    }

    return NULL;
}

const wlm_egl_format_t * wlm_egl_formats_find_spa(enum spa_video_format spa_format) {
    const wlm_egl_format_t * format = formats;
    while (format->bpp != -1U) {
        if (format->spa_format == spa_format) {
            return format;
        }

        format++;
    }

    return NULL;
}
