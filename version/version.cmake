if(NOT ${CMAKE_ARGC} EQUAL 6 OR NOT DEFINED REPO_DIR)
    message(FATAL_ERROR "usage: cmake -DREPO_DIR=<repo-dir> -P version.cmake <target> <template.h.in>")
endif()

set(version-header "${CMAKE_ARGV4}")
set(version-template "${CMAKE_ARGV5}")
set(repo-dir "${REPO_DIR}")

if(IS_DIRECTORY "${repo-dir}/.git")
    execute_process(
        COMMAND git describe --long --tags
        WORKING_DIRECTORY "${repo-dir}"
        OUTPUT_VARIABLE VERSION
    )
elseif(EXISTS "${repo-dir}/version.txt")
    file(READ "${repo-dir}/version.txt" VERSION)
else()
    set(VERSION "")
endif()

string(STRIP "${VERSION}" VERSION)
configure_file("${version-template}" "${version-header}" @ONLY)
