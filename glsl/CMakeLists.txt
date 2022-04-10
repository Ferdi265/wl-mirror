add_library(shaders STATIC)
target_include_directories(shaders INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/include/")
file(GLOB shaders "*.glsl")
foreach(shader ${shaders})
    cmake_path(GET shader STEM shader-base)

    set(shader-header "${CMAKE_CURRENT_BINARY_DIR}/include/glsl_${shader-base}.h")
    set(shader-source "${CMAKE_CURRENT_BINARY_DIR}/src/glsl_${shader-base}.c")

    message(STATUS "embedding ${shader-base}.glsl")

    file(READ ${shader} shader-data HEX)
    string(REPEAT "[0-9a-f]" 2 byte-regex)
    string(REPEAT "[0-9a-f]" 16 line-regex)
    string(REGEX REPLACE "(${line-regex})" "\\1\n    " shader-data "${shader-data}")
    string(REGEX REPLACE "(${byte-regex})" "0x\\1, " shader-data "${shader-data}")

    set(FILENAME "${shader-base}")
    set(DATA "${shader-data}")
    configure_file(embed.h.in "${shader-header}" @ONLY)
    configure_file(embed.c.in "${shader-source}" @ONLY)

    set_source_files_properties("${shader-header}" PROPERTIES GENERATED 1)
    set_source_files_properties("${shader-source}" PROPERTIES GENERATED 1)

    target_sources(shaders PRIVATE "${shader-source}")
endforeach()
