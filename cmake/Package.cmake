# Shared frontend target + dist zip. Call after ROMBUNDLER_PLATFORM_* and
# ROMBUNDLER_INSTALL_* are set. FILES are extra copies next to README/COPYING/INSTALL.md.

function (rombundler_add_frontend)
  add_executable (rombundler ${ROMBUNDLER_SOURCES} ${ROMBUNDLER_PLATFORM_SOURCES})
  target_include_directories (rombundler PRIVATE
    ${ROMBUNDLER_PLATFORM_INCLUDES}
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/include"
  )
  target_compile_options (rombundler PRIVATE -Wall -O3)
endfunction ()

function (rombundler_package_dist)
  cmake_parse_arguments (P "" "COMMENT" "FILES" ${ARGN})
  if (NOT P_COMMENT)
    set (P_COMMENT "Packaging dist/${BUNDLE_NAME}.zip")
  endif ()

  # configure_file @ONLY reads this function's scope; copy install strings in.
  set (ROMBUNDLER_INSTALL_PLATFORM "${ROMBUNDLER_INSTALL_PLATFORM}")
  set (ROMBUNDLER_INSTALL_STEPS "${ROMBUNDLER_INSTALL_STEPS}")
  set (ROMBUNDLER_INSTALL_DATA "${ROMBUNDLER_INSTALL_DATA}")
  set (ROMBUNDLER_INSTALL_LAYOUT "${ROMBUNDLER_INSTALL_LAYOUT}")

  configure_file (
    "${CMAKE_SOURCE_DIR}/cmake/INSTALL.md.in"
    "${CMAKE_BINARY_DIR}/INSTALL.md"
    @ONLY
  )

  set (_cmds
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${STAGE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${STAGE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${DIST_DIR}"
  )
  foreach (_f IN LISTS P_FILES)
    list (APPEND _cmds COMMAND "${CMAKE_COMMAND}" -E copy "${_f}" "${STAGE_DIR}/")
  endforeach ()
  list (APPEND _cmds
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/INSTALL.md" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/README.md" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/COPYING" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}"
            -D "STAGE_DIR=${STAGE_DIR}"
            -D "ZIP=${BUNDLE_ZIP}"
            -P "${CMAKE_SOURCE_DIR}/cmake/package_zip.cmake"
  )

  add_custom_command (TARGET rombundler POST_BUILD
    ${_cmds}
    COMMENT "${P_COMMENT}"
    VERBATIM
  )
endfunction ()
