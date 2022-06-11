if(NOT ${CMAKE_ARGC} EQUAL 6)
    message(FATAL_ERROR "usage: cmake -P embed.cmake <target> <template.c.in> <generated.c>")
endif()

set(shader "${CMAKE_ARGV3}")
set(shader-template "${CMAKE_ARGV4}")
set(shader-source "${CMAKE_ARGV5}")

get_filename_component(shader-base "${shader}" NAME_WE)

file(READ ${shader} shader-data HEX)
string(REPEAT "[0-9a-f]" 2 byte-regex)
string(REPEAT "[0-9a-f]" 16 line-regex)
string(REGEX REPLACE "(${line-regex})" "\\1\n    " shader-data "${shader-data}")
string(REGEX REPLACE "(${byte-regex})" "0x\\1, " shader-data "${shader-data}")

set(FILENAME "${shader-base}")
set(DATA "${shader-data}")
configure_file("${shader-template}" "${shader-source}" @ONLY)
