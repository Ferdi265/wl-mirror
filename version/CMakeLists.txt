file(TOUCH ${CMAKE_CURRENT_BINARY_DIR}/dummy.c)
add_library(version STATIC ${CMAKE_CURRENT_BINARY_DIR}/dummy.c)
target_include_directories(version INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/include/")

set(version-header "${CMAKE_CURRENT_BINARY_DIR}/include/version.h")
set(version-template "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in")

add_custom_target(gen-version
    COMMAND "${CMAKE_COMMAND}" "-DREPO_DIR=${CMAKE_SOURCE_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/version.cmake" "${version-header}" "${version-template}"
)

set_source_files_properties("${version-header}" PROPERTIES GENERATED 1)
add_dependencies(version gen-version)
