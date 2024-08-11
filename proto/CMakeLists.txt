# detect wayland-protocols location
set(wl-protocols-found FALSE)
if(NOT ${FORCE_SYSTEM_WL_PROTOCOLS} AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wayland-protocols/.git")
    message(STATUS "using wayland-protocols from submodule")
    set(WL_PROTOCOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/wayland-protocols/")
    set(wl-protocols-found TRUE)
endif()

if (NOT ${wl-protocols-found} AND IS_DIRECTORY ${WL_PROTOCOL_DIR})
    message(STATUS "using system wayland-protocols at ${WL_PROTOCOL_DIR}")
    set(wl-protocols-found TRUE)
endif()

if (NOT ${wl-protocols-found})
    message(STATUS "error: wayland-protocols not found")
endif()

# detect wlr-protocols location
set(wlr-protocols-found FALSE)
if(NOT ${FORCE_SYSTEM_WLR_PROTOCOLS} AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wlr-protocols/.git")
    message(STATUS "using wlr-protocols from submodule")
    set(WLR_PROTOCOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/wlr-protocols/")
    set(wlr-protocols-found TRUE)
endif()

if (NOT ${wlr-protocols-found} AND IS_DIRECTORY ${WLR_PROTOCOL_DIR})
    message(STATUS "using system wlr-protocols at ${WLR_PROTOCOL_DIR}")
    set(wlr-protocols-found TRUE)
endif()

if (NOT ${wlr-protocols-found})
    message(STATUS "error: wlr-protocols not found")
endif()

if (NOT ${wl-protocols-found} OR NOT ${wlr-protocols-found})
    message(FATAL_ERROR "wayland-protocols or wlr-protocols not found")
endif()

# wayland protocol wrapper generation with wayland-scanner
set(protocols-found TRUE)
add_library(protocols STATIC)
target_include_directories(protocols INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/include/")
target_link_libraries(protocols PRIVATE proto_deps)
foreach(proto ${PROTOCOLS})
    get_filename_component(proto-base "${proto}" NAME_WE)
    set(wl-proto-file "${WL_PROTOCOL_DIR}/${proto}")
    set(wlr-proto-file "${WLR_PROTOCOL_DIR}/${proto}")

    if(EXISTS ${wl-proto-file})
        message(STATUS "using ${proto} from wayland-protocols")
        set(proto-file ${wl-proto-file})
    elseif(EXISTS ${wlr-proto-file})
        message(STATUS "using ${proto} from wlr-protocols")
        set(proto-file ${wlr-proto-file})
    else()
        message(STATUS "error: protocol ${proto} not found")
        set(protocols-found FALSE)
    endif()

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/wlm/proto")
    set(proto-header "${CMAKE_CURRENT_BINARY_DIR}/include/wlm/proto/${proto-base}.h")
    set(proto-source "${CMAKE_CURRENT_BINARY_DIR}/src/${proto-base}.c")

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src/")
    add_custom_command(
        OUTPUT "${proto-header}"
        MAIN_DEPENDENCY "${proto-file}"
        COMMAND "${WAYLAND_SCANNER}" client-header "${proto-file}" "${proto-header}"
    )
    add_custom_command(
        OUTPUT "${proto-source}"
        MAIN_DEPENDENCY "${proto-file}"
        COMMAND "${WAYLAND_SCANNER}" private-code "${proto-file}" "${proto-source}"
    )
    add_custom_target(gen-${proto-base} DEPENDS "${proto-header}" "${proto-source}")

    set_source_files_properties("${proto-header}" PROPERTIES GENERATED 1)
    set_source_files_properties("${proto-source}" PROPERTIES GENERATED 1)

    add_dependencies(protocols gen-${proto-base})
    target_sources(protocols PRIVATE "${proto-source}")
endforeach()

if(NOT ${protocols-found})
    message(FATAL_ERROR "some protocols could not be found")
endif()
