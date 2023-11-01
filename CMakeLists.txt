cmake_minimum_required(VERSION 3.18)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(wl-mirror C)

# options
option(INSTALL_EXAMPLE_SCRIPTS "install wl-mirror example scripts" OFF)
option(INSTALL_DOCUMENTATION "install wl-mirror manual pages" OFF)
option(WITH_LIBDECOR "use libdecor for window decoration" OFF)

# wayland protocols needed by wl-mirror
option(FORCE_SYSTEM_WL_PROTOCOLS "force use of system wayland-protocols" OFF)
option(FORCE_SYSTEM_WLR_PROTOCOLS "force use of system wlr-protocols" OFF)
set(WL_PROTOCOL_DIR "/usr/share/wayland-protocols/" CACHE STRING "wayland-protocols directory")
set(WLR_PROTOCOL_DIR "/usr/share/wlr-protocols/" CACHE STRING "wlr-protocols directory")
set(PROTOCOLS
    "stable/xdg-shell/xdg-shell.xml"
    "stable/viewporter/viewporter.xml"
    "staging/fractional-scale/fractional-scale-v1.xml"
    "unstable/xdg-output/xdg-output-unstable-v1.xml"
    "unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml"
    "unstable/wlr-export-dmabuf-unstable-v1.xml"
    "unstable/wlr-screencopy-unstable-v1.xml"
)

# required dependencies
add_subdirectory(deps)

# wayland protocol wrapper generation with wayland-scanner
add_subdirectory(proto)

# shader file embedding
add_subdirectory(glsl)

# version embedding
add_subdirectory(version)

# manual pages
add_subdirectory(man)
if (${BUILD_DOCUMENTATION})
    build_scdoc_man_page(wl-mirror 1)
    build_scdoc_man_page(wl-present 1)
endif()

# main target
file(GLOB sources CONFIGURE_DEPENDS src/*.c)
add_executable(wl-mirror ${sources})
target_compile_options(wl-mirror PRIVATE -Wall -Wextra)
target_include_directories(wl-mirror PRIVATE include/)
target_link_libraries(wl-mirror PRIVATE deps protocols shaders version)

# installation rules
include(GNUInstallDirs)

install(TARGETS wl-mirror DESTINATION "${CMAKE_INSTALL_BINDIR}")
if (${INSTALL_EXAMPLE_SCRIPTS})
    install(PROGRAMS scripts/wl-present DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

if (${INSTALL_DOCUMENTATION})
    install_scdoc_man_page(wl-mirror 1)
    if (${INSTALL_EXAMPLE_SCRIPTS})
        install_scdoc_man_page(wl-present 1)
    endif()
endif()
