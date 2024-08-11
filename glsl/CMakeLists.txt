add_library(shaders STATIC)
target_include_directories(shaders PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include/")
file(GLOB shaders CONFIGURE_DEPENDS "*.glsl")
foreach(shader ${shaders})
    get_filename_component(shader-base "${shader}" NAME_WE)

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/wlm/glsl")
    set(shader-template "${CMAKE_CURRENT_SOURCE_DIR}/embed.c.in")
    set(shader-header "${CMAKE_CURRENT_BINARY_DIR}/include/wlm/glsl/${shader-base}.h")
    set(shader-source "${CMAKE_CURRENT_BINARY_DIR}/src/glsl_${shader-base}.c")

    message(STATUS "embedding ${shader-base}.glsl")

    add_custom_command(
        OUTPUT "${shader-source}"
        MAIN_DEPENDENCY "${shader}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/embed.cmake" "${shader-template}"
        COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/embed.cmake" "${shader}" "${shader-template}" "${shader-source}"
    )
    add_custom_target(gen-${shader-base} DEPENDS "${shader-source}")

    set(FILENAME "${shader-base}")
    set(DATA "${shader-data}")
    configure_file(embed.h.in "${shader-header}" @ONLY)

    set_source_files_properties("${shader-header}" PROPERTIES GENERATED 1)
    set_source_files_properties("${shader-source}" PROPERTIES GENERATED 1)

    add_dependencies(shaders gen-${shader-base})
    target_sources(shaders PRIVATE "${shader-source}")
endforeach()
