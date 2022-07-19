cmake_minimum_required(VERSION 3.18)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(wl-mirror C)

# options
option(INSTALL_EXAMPLE_SCRIPTS "install wl-mirror example scripts" OFF)
option(INSTALL_DOCUMENTATION "install wl-mirror manual pages" OFF)

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
    "unstable/wlr-screencopy-unstable-v1.xml"
)

# wayland protocol wrapper generation with wayland-scanner
add_subdirectory(proto)

# shader file embedding
add_subdirectory(glsl)

# version embedding
add_subdirectory(version)

# manual pages
find_program(SCDOC scdoc)
if (SCDOC)
    add_subdirectory(man)
    build_scdoc_man_page(wl-mirror 1)
    build_scdoc_man_page(wl-present 1)
endif()

# main target
file(GLOB sources src/*.c)
add_executable(wl-mirror ${sources})
target_compile_options(wl-mirror PRIVATE -Wall -Wextra)
target_link_libraries(wl-mirror PRIVATE wayland-client wayland-egl EGL GLESv2 protocols shaders version)
target_include_directories(wl-mirror PRIVATE include/)

# installation rules
install(TARGETS wl-mirror DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
if (${INSTALL_EXAMPLE_SCRIPTS})
    install(PROGRAMS scripts/wl-present DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (SCDOC AND ${INSTALL_DOCUMENTATION})
    install_scdoc_man_page(wl-mirror 1)
    if (${INSTALL_EXAMPLE_SCRIPTS})
        install_scdoc_man_page(wl-present 1)
    endif()
endif()
