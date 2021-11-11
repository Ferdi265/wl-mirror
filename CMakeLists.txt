cmake_minimum_required(VERSION 3.20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(wl-mirror C)

# wayland protocols needed by wl-mirror
option(FORCE_SYSTEM_WL_PROTOCOLS "force use of system wayland-protocols" OFF)
option(FORCE_SYSTEM_WLR_PROTOCOLS "force use of system wlr-protocols" OFF)
set(WL_PROTOCOL_DIR "/usr/share/wayland-protocols/" CACHE STRING "wayland-protocols directory")
set(WLR_PROTOCOL_DIR "/usr/share/wlr-protocols/" CACHE STRING "wlr-protocols directory")
set(PROTOCOLS
    "stable/xdg-shell/xdg-shell.xml"
    "unstable/xdg-output/xdg-output-unstable-v1.xml"
    "unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml"
    "unstable/wlr-export-dmabuf-unstable-v1.xml"
)

# wayland protocol wrapper generation with wayland-scanner
add_subdirectory(proto)

# main target
file(GLOB sources src/*.c)
add_executable(wl-mirror ${sources})
target_link_libraries(wl-mirror PRIVATE wayland-client wayland-egl EGL GLESv2 protocols)
target_include_directories(wl-mirror PRIVATE include/)
