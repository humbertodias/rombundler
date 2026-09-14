# GLFW + OpenAL ports (Linux, Windows, macOS). Toolchains live in cmake/toolchains/.

set (ROMBUNDLER_PLATFORM_SOURCES
  "${CMAKE_SOURCE_DIR}/src/glad.c"
  "${CMAKE_SOURCE_DIR}/src/platforms/desktop/platform_glfw.c"
  "${CMAKE_SOURCE_DIR}/src/platforms/desktop/audio.c"
  "${CMAKE_SOURCE_DIR}/src/platforms/desktop/input.c"
)
set (ROMBUNDLER_PLATFORM_INCLUDES
  "${CMAKE_SOURCE_DIR}/src/platforms/desktop"
)
