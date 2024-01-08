#include <wlm/context.h>

const wlm_wayland_registry_bind_multiple_t wlm_wayland_registry_bind_multiple[] = {
    WLM_WAYLAND_REGISTRY_BIND_MULTIPLE(wl_output_interface,                         4, true, wlm_wayland_output_on_add, wlm_wayland_output_on_remove),
    WLM_WAYLAND_REGISTRY_BIND_MULTIPLE_END
};

const wlm_wayland_registry_bind_singleton_t wlm_wayland_registry_bind_singleton[] = {
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(wl_compositor_interface,                    4, true,    ctx_t, wl.protocols.compositor),
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(wp_viewporter_interface,                    1, true,    ctx_t, wl.protocols.viewporter),
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(xdg_wm_base_interface,                      2, true,    ctx_t, wl.protocols.xdg_wm_base),
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(zxdg_output_manager_v1_interface,           3, true,    ctx_t, wl.protocols.xdg_output_manager),
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(wl_shm_interface,                           1, false,   ctx_t, wl.protocols.shm),
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(wp_fractional_scale_manager_v1_interface,   1, false,   ctx_t, wl.protocols.fractional_scale_manager),
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(zwlr_export_dmabuf_manager_v1_interface,    1, false,   ctx_t, wl.protocols.export_dmabuf_manager),
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(zwlr_screencopy_manager_v1_interface,       3, false,   ctx_t, wl.protocols.screencopy_manager),
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON_END
};
