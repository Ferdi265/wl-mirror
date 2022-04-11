add_library(version STATIC)
target_include_directories(version INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/include/")

set(dummy-source "${CMAKE_CURRENT_BINARY_DIR}/src/dummy.c")
set(version-header "${CMAKE_CURRENT_BINARY_DIR}/include/version.h")
set(version-template "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in")

add_custom_target(gen-version
    COMMAND "${CMAKE_COMMAND}" "-DREPO_DIR=${CMAKE_SOURCE_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/version.cmake" "${version-header}" "${version-template}"
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/")
file(WRITE "${dummy-source}" "")

set_source_files_properties("${dummy-source}" PROPERTIES GENERATED 1)
set_source_files_properties("${version-header}" PROPERTIES GENERATED 1)

add_dependencies(version gen-version)
target_sources(version PRIVATE "${dummy-source}")
