# Static GLFW + OpenAL-Soft via FetchContent. Used by every port.

include (FetchContent)

set (BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set (GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set (GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set (GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set (GLFW_INSTALL OFF CACHE BOOL "" FORCE)
if (UNIX AND NOT APPLE)
  set (GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
endif ()

set (LIBTYPE STATIC CACHE STRING "" FORCE)
set (ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
set (ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
set (ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
set (ALSOFT_INSTALL_HRTF_DATA OFF CACHE BOOL "" FORCE)
set (ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "" FORCE)
set (ALSOFT_INSTALL_EXAMPLES OFF CACHE BOOL "" FORCE)
set (ALSOFT_INSTALL_UTILS OFF CACHE BOOL "" FORCE)

FetchContent_Declare (
  glfw
  URL "https://github.com/glfw/glfw/archive/refs/tags/${ROMBUNDLER_GLFW_VERSION}.tar.gz"
)
FetchContent_Declare (
  openal
  URL "https://github.com/kcat/openal-soft/archive/refs/tags/${ROMBUNDLER_OPENAL_VERSION}.tar.gz"
)

FetchContent_MakeAvailable (glfw openal)

if (APPLE AND TARGET OpenAL)
  get_target_property (_openal_if OpenAL INTERFACE_LINK_LIBRARIES)
  if (_openal_if)
    list (FILTER _openal_if EXCLUDE REGEX "atomic")
    set_property (TARGET OpenAL PROPERTY INTERFACE_LINK_LIBRARIES "${_openal_if}")
  endif ()
endif ()
