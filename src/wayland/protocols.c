#include <wlm/context.h>

const wlm_wayland_registry_bind_multiple_t wlm_wayland_registry_bind_multiple[] = {
    // need wl_output::done from v2
    WLM_WAYLAND_REGISTRY_BIND_MULTIPLE(wl_output_interface,                         2, 4, true, wlm_wayland_output_on_add, wlm_wayland_output_on_remove), // TODO: old version = 3
    WLM_WAYLAND_REGISTRY_BIND_MULTIPLE(wl_seat_interface,                           1, 1, true, wlm_wayland_seat_on_add, wlm_wayland_seat_on_remove), // TODO: old version = 1
    WLM_WAYLAND_REGISTRY_BIND_MULTIPLE_END
};

const wlm_wayland_registry_bind_singleton_t wlm_wayland_registry_bind_singleton[] = {
    // required
    // need wl_surface::set_buffer_scale from v3
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(wl_compositor_interface,                    3, 6, true,  wl.protocols.compositor), // TODO: old version = 4
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(wp_viewporter_interface,                    1, 1, true,  wl.protocols.viewporter), // TODO: old version = 1
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(xdg_wm_base_interface,                      1, 6, true,  wl.protocols.xdg_wm_base), // TODO: old version = 2
    // need xdg_output::name from v2
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(zxdg_output_manager_v1_interface,           2, 3, true,  wl.protocols.xdg_output_manager), // TODO: old version = 2

    // optional
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(wl_shm_interface,                           1, 1, false, wl.protocols.shm), // TODO: old version = 1
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(zwp_linux_dmabuf_v1_interface,              4, 4, false, wl.protocols.linux_dmabuf), // TODO: old version = 4
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(wp_fractional_scale_manager_v1_interface,   1, 1, false, wl.protocols.fractional_scale_manager), // TODO: old version = 1

    // export-dmabuf backend
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(zwlr_export_dmabuf_manager_v1_interface,    1, 1, false, wl.protocols.export_dmabuf_manager), // TODO: old version = 1

    // screencopy backend
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(zwlr_screencopy_manager_v1_interface,       1, 3, false, wl.protocols.screencopy_manager), // TODO: old version = 3

    // extcopy backend
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(ext_image_copy_capture_manager_v1_interface,                    1, 1, false, wl.protocols.copy_capture_manager), // TODO: old vesion = 1
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(ext_output_image_capture_source_manager_v1_interface,           1, 1, false, wl.protocols.output_capture_source_manager), // TODO: old version = 1
    WLM_WAYLAND_REGISTRY_BIND_SINGLETON(ext_foreign_toplevel_image_capture_source_manager_v1_interface, 1, 1, false, wl.protocols.toplevel_capture_source_manager), // TODO: old version = 1

    WLM_WAYLAND_REGISTRY_BIND_SINGLETON_END
};
