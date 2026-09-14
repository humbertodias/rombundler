# Nintendo Switch port: sources and include path. Toolchain is cmake/toolchains/devkita64-libnx.cmake.
# GL loader is mesa's libglad (OpenGL 4.3 core), not src/glad.c (desktop 2.1).

set (ROMBUNDLER_PLATFORM_SOURCES
  "${CMAKE_SOURCE_DIR}/src/platforms/switch/platform.c"
  "${CMAKE_SOURCE_DIR}/src/platforms/switch/audio.c"
  "${CMAKE_SOURCE_DIR}/src/platforms/switch/input.c"
)
set (ROMBUNDLER_PLATFORM_LIBS glad)
if (ROMBUNDLER_CORE_LIBRARY)
  list (APPEND ROMBUNDLER_PLATFORM_LIBS "${ROMBUNDLER_CORE_LIBRARY}")
  list (APPEND ROMBUNDLER_PLATFORM_SOURCES "${CMAKE_SOURCE_DIR}/src/platforms/switch/libretro_compat.c")
else ()
  list (APPEND ROMBUNDLER_PLATFORM_SOURCES "${CMAKE_SOURCE_DIR}/src/platforms/switch/dummy_core.c")
  set (ROMBUNDLER_PLATFORM_DEFINES ROMBUNDLER_DUMMY_CORE)
endif ()
set (ROMBUNDLER_PLATFORM_INCLUDES
  "${CMAKE_SOURCE_DIR}/src/platforms/switch"
)
