cmake_minimum_required (VERSION 3.24)

project (rombundler C CXX)

set (CMAKE_C_STANDARD 11)
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

set (ROMBUNDLER_VERSION "devel" CACHE STRING "Bundle version string")
set (ROMBUNDLER_GLFW_VERSION "3.4" CACHE STRING "GLFW source tag")
set (ROMBUNDLER_OPENAL_VERSION "1.24.2" CACHE STRING "OpenAL-Soft source tag")
set (ROMBUNDLER_CORE_LIBRARY "" CACHE FILEPATH "Optional static libretro core (.a) for Switch")

if (NINTENDO_SWITCH)
  set (ROMBUNDLER_PORT_OS "Switch")
  set (ROMBUNDLER_ARCH "aarch64")
  set (ROMBUNDLER_PLATFORM_FILE "${CMAKE_SOURCE_DIR}/cmake/platforms/NintendoSwitch.cmake")
elseif (WIN32)
  set (ROMBUNDLER_PORT_OS "Windows")
  set (ROMBUNDLER_ARCH "x86_64")
  set (ROMBUNDLER_PLATFORM_FILE "${CMAKE_SOURCE_DIR}/cmake/platforms/Desktop.cmake")
elseif (APPLE)
  set (ROMBUNDLER_PORT_OS "OSX")
  set (ROMBUNDLER_PLATFORM_FILE "${CMAKE_SOURCE_DIR}/cmake/platforms/Desktop.cmake")
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
  set (ROMBUNDLER_PLATFORM_FILE "${CMAKE_SOURCE_DIR}/cmake/platforms/Desktop.cmake")
endif ()

if (NOT NINTENDO_SWITCH)
  include ("${CMAKE_SOURCE_DIR}/cmake/FetchDeps.cmake")
endif ()

set (ROMBUNDLER_SOURCES
  src/main.c
  src/config.c
  src/core.c
  src/video.c
  src/options.c
  src/ini.c
  src/utils.c
  src/srm.c
)

include ("${ROMBUNDLER_PLATFORM_FILE}")

add_executable (rombundler ${ROMBUNDLER_SOURCES} ${ROMBUNDLER_PLATFORM_SOURCES})

target_include_directories (rombundler PRIVATE
  ${ROMBUNDLER_PLATFORM_INCLUDES}
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/include"
)
target_compile_options (rombundler PRIVATE -Wall -O3)

set (BUNDLE_NAME "ROMBundler-${ROMBUNDLER_PORT_OS}-${ROMBUNDLER_VERSION}-${ROMBUNDLER_ARCH}")
set (DIST_DIR "${CMAKE_SOURCE_DIR}/dist")
set (STAGE_DIR "${DIST_DIR}/${BUNDLE_NAME}")
set (BUNDLE_ZIP "${DIST_DIR}/${BUNDLE_NAME}.zip")

include ("${ROMBUNDLER_PLATFORM_FILE}")
