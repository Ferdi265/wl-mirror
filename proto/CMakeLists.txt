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
target_include_directories(protocols INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/proto/include/")
foreach(proto ${PROTOCOLS})
    cmake_path(GET proto STEM protobase)
    cmake_path(APPEND WL_PROTOCOL_DIR ${proto} OUTPUT_VARIABLE wl_protofile)
    cmake_path(APPEND WLR_PROTOCOL_DIR ${proto} OUTPUT_VARIABLE wlr_protofile)

    if(EXISTS ${wl_protofile})
        message(STATUS "using ${proto} from wayland-protocols")
        set(protofile ${wl_protofile})
    elseif(EXISTS ${wlr_protofile})
        message(STATUS "using ${proto} from wlr-protocols")
        set(protofile ${wlr_protofile})
    else()
        message(STATUS "error: protocol ${proto} not found")
        set(protocols-found FALSE)
    endif()

    set(protoheader "${CMAKE_CURRENT_BINARY_DIR}/proto/include/${protobase}.h")
    set(protosource "${CMAKE_CURRENT_BINARY_DIR}/proto/src/${protobase}.c")

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/proto/include/")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/proto/src/")
    add_custom_command(
        OUTPUT "${protoheader}"
        MAIN_DEPENDENCY "${protofile}"
        COMMAND wayland-scanner client-header "${protofile}" "${protoheader}"
    )
    add_custom_command(
        OUTPUT "${protosource}"
        MAIN_DEPENDENCY "${protofile}"
        COMMAND wayland-scanner private-code "${protofile}" "${protosource}"
    )
    add_custom_target(gen-${protobase} DEPENDS "${protoheader}" "${protosource}")

    set_source_files_properties("${protoheader}" PROPERTIES GENERATED 1)
    set_source_files_properties("${protosource}" PROPERTIES GENERATED 1)

    add_dependencies(protocols gen-${protobase})
    target_sources(protocols PRIVATE "${protosource}")
endforeach()

if(NOT ${protocols-found})
    message(FATAL_ERROR "some protocols could not be found")
endif()
