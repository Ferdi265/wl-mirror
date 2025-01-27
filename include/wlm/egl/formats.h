#ifndef WLM_EGL_FORMATS_H_
#define WLM_EGL_FORMATS_H_

#include <stdint.h>
#include <wayland-client-protocol.h>
#include <GLES2/gl2.h>

#ifdef WITH_LIBDRM
#include <libdrm/drm_fourcc.h>
#define DRM_FORMAT(a) DRM_FORMAT_ ## a
#else
// fallback for use without libdrm
// wl_shm formats are defined to be equal to drm except ARGB888 and XRGB8888
#define DRM_FORMAT(a) ( \
    (WL_SHM_FORMAT_ ## a) == WL_SHM_FORMAT_ARGB8888 ? 0x34325241 : \
    (WL_SHM_FORMAT_ ## a) == WL_SHM_FORMAT_XRGB8888 ? 0x34325258 : \
    (WL_SHM_FORMAT_ ## a) \
)
#endif

#ifdef WITH_XDG_PORTAL_BACKEND
#include <spa/param/video/raw.h>
#define SPA_FORMAT(a) SPA_VIDEO_FORMAT_ ## a
#else
// fallback for use without xdg portal backend
enum spa_video_format {
    SPA_VIDEO_FORMAT_UNKNOWN
};
#define SPA_FORMAT(a) SPA_VIDEO_FORMAT_UNKNOWN
#endif

typedef struct {
    enum wl_shm_format wl_shm_format;
    uint32_t drm_format;
    enum spa_video_format spa_format;
    uint32_t bpp;
    GLint gl_format;
    GLint gl_type;
} wlm_egl_format_t;

const wlm_egl_format_t * wlm_egl_formats_find_shm(enum wl_shm_format shm_format);
const wlm_egl_format_t * wlm_egl_formats_find_drm(uint32_t drm_format);
const wlm_egl_format_t * wlm_egl_formats_find_spa(enum spa_video_format spa_format);

#endif
