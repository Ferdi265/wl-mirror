# check if documentation must be built
if (${INSTALL_DOCUMENTATION})
    set(SCDOC_REQUIRED REQUIRED)
else()
    set(SCDOC_REQUIRED)
endif()

# determine if documentation can be built
find_program(SCDOC scdoc ${SCDOC_REQUIRED})
if (SCDOC)
    set(BUILD_DOCUMENTATION ON PARENT_SCOPE)
else()
    set(BUILD_DOCUMENTATION OFF PARENT_SCOPE)
endif()

function(build_scdoc_man_page name section)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/man/")
    set(man-src "${CMAKE_SOURCE_DIR}/man/${name}.${section}.scd")
    set(man-bin "${CMAKE_BINARY_DIR}/man/${name}.${section}")
    add_custom_command(
        OUTPUT "${man-bin}"
        COMMAND scdoc < "${man-src}" > "${man-bin}"
    )
    add_custom_target(gen-man-${name}-${section} ALL DEPENDS "${man-bin}")
endfunction()

function(install_scdoc_man_page name section)
    set(man-bin "${CMAKE_BINARY_DIR}/man/${name}.${section}")
    install(FILES "${man-bin}" DESTINATION "${CMAKE_INSTALL_MANDIR}/man${section}/")
endfunction()
