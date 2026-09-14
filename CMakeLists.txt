cmake_minimum_required (VERSION 3.24)

project (rombundler C CXX)

set (CMAKE_C_STANDARD 11)
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

set (ROMBUNDLER_VERSION "devel" CACHE STRING "Bundle version string")
set (ROMBUNDLER_GLFW_VERSION "3.4" CACHE STRING "GLFW source tag")
set (ROMBUNDLER_OPENAL_VERSION "1.24.2" CACHE STRING "OpenAL-Soft source tag")

if (WIN32)
  set (ROMBUNDLER_PORT_OS "Windows")
  set (ROMBUNDLER_ARCH "x86_64")
elseif (APPLE)
  set (ROMBUNDLER_PORT_OS "OSX")
  if (CMAKE_OSX_ARCHITECTURES)
    list (GET CMAKE_OSX_ARCHITECTURES 0 ROMBUNDLER_ARCH)
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set (ROMBUNDLER_ARCH "arm64")
  else ()
    set (ROMBUNDLER_ARCH "x86_64")
  endif ()
else ()
  set (ROMBUNDLER_PORT_OS "Linux")
  set (ROMBUNDLER_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif ()

include ("${CMAKE_SOURCE_DIR}/cmake/FetchDeps.cmake")

add_executable (rombundler
  main.c
  glad.c
  config.c
  core.c
  audio.c
  video.c
  input.c
  options.c
  ini.c
  utils.c
  srm.c
)

target_include_directories (rombundler PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/deps/include"
)

set (_openal_target OpenAL)
if (TARGET OpenAL::OpenAL)
  set (_openal_target OpenAL::OpenAL)
endif ()

target_link_libraries (rombundler PRIVATE glfw ${_openal_target})

if (WIN32)
  target_compile_definitions (rombundler PRIVATE AL_LIBTYPE_STATIC)
  set_target_properties (rombundler PROPERTIES WIN32_EXECUTABLE TRUE)
  target_link_options (rombundler PRIVATE
    -static-libgcc
    -static-libstdc++
  )
  target_link_libraries (rombundler PRIVATE
    gdi32 user32 shell32 imm32 ole32 uuid winmm ksuser avrt setupapi cfgmgr32 opengl32
  )
elseif (APPLE)
  target_link_libraries (rombundler PRIVATE
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreFoundation"
    "-framework CoreVideo"
    "-framework OpenGL"
    "-framework AudioToolbox"
    "-framework CoreAudio"
    "-framework AudioUnit"
    c++
  )
  find_program (OSX_CODESIGN osxcross-codesign)
  if (OSX_CODESIGN)
    add_custom_command (TARGET rombundler POST_BUILD
      COMMAND "${OSX_CODESIGN}" -s - -f "$<TARGET_FILE:rombundler>"
      VERBATIM
    )
  endif ()
else ()
  target_link_libraries (rombundler PRIVATE m dl pthread stdc++)
endif ()

if (NOT APPLE)
  include (CheckIPOSupported)
  check_ipo_supported (RESULT _ipo)
  if (_ipo)
    set_property (TARGET rombundler PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif ()
endif ()

target_compile_options (rombundler PRIVATE -Wall -O3)

set (BUNDLE_NAME "ROMBundler-${ROMBUNDLER_PORT_OS}-${ROMBUNDLER_VERSION}-${ROMBUNDLER_ARCH}")
set (DIST_DIR "${CMAKE_SOURCE_DIR}/dist")
set (STAGE_ROOT "${CMAKE_BINARY_DIR}/stage")
set (STAGE_DIR "${STAGE_ROOT}/${BUNDLE_NAME}")

add_custom_command (TARGET rombundler POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${STAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${STAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${DIST_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:rombundler>" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/config.ini" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/README.md" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/COPYING" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E chdir "${STAGE_ROOT}"
          "${CMAKE_COMMAND}" -E tar "cf" "${DIST_DIR}/${BUNDLE_NAME}.zip" --format=zip -- "${BUNDLE_NAME}"
  COMMENT "Packaging dist/${BUNDLE_NAME}.zip"
  VERBATIM
)
