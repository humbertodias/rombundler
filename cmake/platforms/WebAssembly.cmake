# WebAssembly (Emscripten). Toolchain: cmake/toolchains/emscripten.cmake.
# GLFW3 + WebGL2/GLES3 + OpenAL Soft (Emscripten ports). No dlopen.

set (ROMBUNDLER_CORE_LIBRARY "" CACHE FILEPATH "Optional static libretro core (.a) for WASM")

set (WASM_PRELOAD "${CMAKE_SOURCE_DIR}/src/platforms/wasm/preload")

set (ROMBUNDLER_PLATFORM_SOURCES
  "${CMAKE_SOURCE_DIR}/src/platforms/wasm/platform.c"
  "${CMAKE_SOURCE_DIR}/src/platforms/wasm/audio.c"
  "${CMAKE_SOURCE_DIR}/src/platforms/desktop/input.c"
)
set (ROMBUNDLER_PLATFORM_LIBS)
if (ROMBUNDLER_CORE_LIBRARY)
  list (APPEND ROMBUNDLER_PLATFORM_LIBS "${ROMBUNDLER_CORE_LIBRARY}")
  list (APPEND ROMBUNDLER_PLATFORM_SOURCES "${CMAKE_SOURCE_DIR}/src/platforms/static/libretro_compat.c")
else ()
  list (APPEND ROMBUNDLER_PLATFORM_SOURCES "${CMAKE_SOURCE_DIR}/src/dummy_core.c")
  set (ROMBUNDLER_PLATFORM_DEFINES ROMBUNDLER_DUMMY_CORE)
endif ()
set (ROMBUNDLER_PLATFORM_INCLUDES
  "${CMAKE_SOURCE_DIR}/src/platforms/wasm"
  "${CMAKE_SOURCE_DIR}/src/platforms/static"
  "${CMAKE_SOURCE_DIR}/src/platforms/gles"
  "${CMAKE_SOURCE_DIR}/src/platforms/desktop"
)
rombundler_add_frontend ()

set_target_properties (rombundler PROPERTIES
  OUTPUT_NAME "rombundler"
  SUFFIX ".html"
)

target_link_options (rombundler PRIVATE
  "SHELL:-sUSE_GLFW=3"
  "SHELL:-sUSE_WEBGL2=1"
  "SHELL:-sFULL_ES3=1"
  "SHELL:-sALLOW_MEMORY_GROWTH=1"
  "SHELL:-sFORCE_FILESYSTEM=1"
  "SHELL:-sINITIAL_MEMORY=67108864"
  "SHELL:-sEXIT_RUNTIME=0"
  "SHELL:--preload-file ${WASM_PRELOAD}/config.ini@/config.ini"
  "SHELL:--preload-file ${WASM_PRELOAD}/dummy.bin@/dummy.bin"
  "SHELL:-lopenal"
)

if (ROMBUNDLER_PLATFORM_DEFINES)
  target_compile_definitions (rombundler PRIVATE ${ROMBUNDLER_PLATFORM_DEFINES})
endif ()
if (ROMBUNDLER_PLATFORM_LIBS)
  target_link_libraries (rombundler PRIVATE ${ROMBUNDLER_PLATFORM_LIBS})
endif ()

set (ROMBUNDLER_INSTALL_PLATFORM "WebAssembly")
set (ROMBUNDLER_INSTALL_STEPS [=[Serve the folder over HTTP (browsers block `file://` WASM). Example: `python3 -m http.server -d . 8080` then open `http://localhost:8080/rombundler.html`.]=])
set (ROMBUNDLER_INSTALL_DATA [=[See `wasm.md` in this zip. Default build uses the dummy core and `/dummy.bin` from the preload package. `core=` does not `dlopen` a `.js`/`.wasm` core. SRAM is `/save.srm` in the Emscripten FS (session only unless you add IDBFS).]=])
set (ROMBUNDLER_INSTALL_LAYOUT [=[Static WASM app: Emscripten GLFW3, WebGL2/GLES3, OpenAL. No runtime `dlopen`.]=])

configure_file (
  "${CMAKE_SOURCE_DIR}/cmake/INSTALL.md.in"
  "${CMAKE_BINARY_DIR}/INSTALL.md"
  @ONLY
)

set (WASM_HTML "${CMAKE_BINARY_DIR}/rombundler.html")
set (WASM_JS "${CMAKE_BINARY_DIR}/rombundler.js")
set (WASM_WASM "${CMAKE_BINARY_DIR}/rombundler.wasm")
set (WASM_DATA "${CMAKE_BINARY_DIR}/rombundler.data")

add_custom_target (rombundler-dist ALL
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${STAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${STAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${DIST_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy "${WASM_HTML}" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${WASM_JS}" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${WASM_WASM}" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${WASM_DATA}" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/INSTALL.md" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/README.md" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/doc/wasm.md" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/COPYING" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}"
          -D "STAGE_DIR=${STAGE_DIR}"
          -D "ZIP=${BUNDLE_ZIP}"
          -P "${CMAKE_SOURCE_DIR}/cmake/package_zip.cmake"
  COMMENT "Packaging dist/${BUNDLE_NAME}.zip (WASM)"
  VERBATIM
)
add_dependencies (rombundler-dist rombundler)
