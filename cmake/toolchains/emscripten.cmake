# Cross-compile with Emscripten (emsdk).
#   cmake --preset wasm
# Expects emcc on PATH and preferably EMSDK (Docker image sets /emsdk).

set (_em_cmake "")
if (DEFINED ENV{EMSDK} AND EXISTS "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
  set (_em_cmake "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
elseif (EXISTS "/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
  set (_em_cmake "/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
endif ()

if (_em_cmake)
  include ("${_em_cmake}")
else ()
  find_program (EMCC emcc REQUIRED)
  find_program (EMXX em++ REQUIRED)
  set (CMAKE_SYSTEM_NAME Emscripten CACHE STRING "" FORCE)
  set (CMAKE_C_COMPILER "${EMCC}" CACHE FILEPATH "" FORCE)
  set (CMAKE_CXX_COMPILER "${EMXX}" CACHE FILEPATH "" FORCE)
  set (CMAKE_C_COMPILER_WORKS TRUE CACHE INTERNAL "")
  set (CMAKE_CXX_COMPILER_WORKS TRUE CACHE INTERNAL "")
endif ()

set (EMSCRIPTEN TRUE CACHE BOOL "Build WebAssembly (Emscripten)" FORCE)
set (CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
