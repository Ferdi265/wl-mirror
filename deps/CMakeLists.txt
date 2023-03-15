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

# find required dependencies
pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET "wayland-client")
pkg_check_modules(WaylandEGL REQUIRED IMPORTED_TARGET "wayland-egl")
pkg_check_modules(EGL REQUIRED IMPORTED_TARGET "egl")
pkg_check_modules(GLESv2 REQUIRED IMPORTED_TARGET "glesv2")

# link dependencies
target_link_libraries(deps INTERFACE
    PkgConfig::WaylandClient PkgConfig::WaylandEGL PkgConfig::EGL PkgConfig::GLESv2
)
target_link_libraries(proto_deps INTERFACE
    PkgConfig::WaylandClient
)
