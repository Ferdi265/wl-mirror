find_package(PkgConfig REQUIRED)

add_library(deps INTERFACE)
add_library(proto_deps INTERFACE)

# helper function for finding one of a list of packages
function(do_pkg_search_module name)
    pkg_search_module(${name} ${ARGN})

    set(module_name "${${name}_MODULE_NAME}")
    set(module_version "${${name}_VERSION}")
    message(STATUS "  Found ${module_name}, version ${module_version}")
endfunction()

# required dependencies
find_library(MATH_LIBRARY m REQUIRED)
pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET "wayland-client")
pkg_check_modules(WaylandEGL REQUIRED IMPORTED_TARGET "wayland-egl")
pkg_check_modules(EGL REQUIRED IMPORTED_TARGET "egl")
pkg_check_modules(GLESv2 REQUIRED IMPORTED_TARGET "glesv2")
pkg_check_modules(WaylandScanner REQUIRED "wayland-scanner")
pkg_get_variable(WAYLAND_SCANNER "wayland-scanner" "wayland_scanner")
set(WAYLAND_SCANNER "${WAYLAND_SCANNER}" PARENT_SCOPE)

# man dependencies
if (${INSTALL_DOCUMENTATION})
    pkg_check_modules(SCDOC REQUIRED "scdoc")
else()
    pkg_check_modules(SCDOC "scdoc")
endif()
pkg_get_variable(SCDOC "scdoc" "scdoc")
set(SCDOC "${SCDOC}" PARENT_SCOPE)

# link dependencies
target_link_libraries(deps INTERFACE
    ${MATH_LIBRARY}
    PkgConfig::WaylandClient PkgConfig::WaylandEGL PkgConfig::EGL PkgConfig::GLESv2
)
target_link_libraries(proto_deps INTERFACE
    PkgConfig::WaylandClient
)

# check for epoll support in libc
include(CheckSymbolExists)
check_symbol_exists("epoll_wait" "sys/epoll.h" has-epoll)
if(NOT has-epoll)
    # use epoll-shim to provide epoll
    pkg_check_modules(EPoll REQUIRED IMPORTED_TARGET "epoll-shim")
    target_link_libraries(deps INTERFACE PkgConfig::EPoll)
endif()

# find xdg-portal dependencies (dbus and pipewire)
if(${WITH_XDG_PORTAL_BACKEND})
    if(${SD_BUS_PROVIDER} STREQUAL AUTO)
        do_pkg_search_module(SDBus REQUIRED IMPORTED_TARGET "libsystemd" "libelogind" "basu")
    else()
        pkg_check_modules(SDBus REQUIRED IMPORTED_TARGET "${SD_BUS_PROVIDER}")
    endif()
    pkg_check_modules(PipeWire REQUIRED IMPORTED_TARGET "libpipewire-0.3")
    target_link_libraries(deps INTERFACE PkgConfig::SDBus PkgConfig::PipeWire)
    target_compile_definitions(deps INTERFACE WITH_XDG_PORTAL_BACKEND)
endif()
