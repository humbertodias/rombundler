# WebAssembly (Emscripten). Toolchain: cmake/toolchains/emscripten.cmake.
# GLFW3 + WebGL2/GLES3 + OpenAL Soft (Emscripten ports).
# Static builds link a core. ROMBUNDLER_WASM_DYNAMIC is the GitHub Pages loader:
# no core, MAIN_MODULE, the page dlopens a dropped side-module .wasm.

set (ROMBUNDLER_CORE_LIBRARY "" CACHE FILEPATH "Optional static libretro core (.a) for WASM")
set (ROMBUNDLER_WASM_ROM "" CACHE FILEPATH "Optional game ROM to preload (VFS path /<basename>)")
set (ROMBUNDLER_WASM_DYNAMIC OFF CACHE BOOL "Pages loader: dlopen a dropped .wasm side module")

set (WASM_PRELOAD "${CMAKE_SOURCE_DIR}/src/platforms/wasm/preload")
set (WASM_SHELL "${CMAKE_SOURCE_DIR}/src/platforms/wasm/shell.html")

set (ROMBUNDLER_PLATFORM_SOURCES
  "${CMAKE_SOURCE_DIR}/src/platforms/wasm/platform.c"
  "${CMAKE_SOURCE_DIR}/src/platforms/wasm/audio.c"
  "${CMAKE_SOURCE_DIR}/src/platforms/desktop/input.c"
)
set (ROMBUNDLER_PLATFORM_LIBS)
if (ROMBUNDLER_WASM_DYNAMIC)
  if (ROMBUNDLER_CORE_LIBRARY)
    message (FATAL_ERROR "ROMBUNDLER_WASM_DYNAMIC cannot link ROMBUNDLER_CORE_LIBRARY")
  endif ()
  if (ROMBUNDLER_WASM_ROM)
    message (FATAL_ERROR "ROMBUNDLER_WASM_DYNAMIC does not bake a ROM; drop it on the page")
  endif ()
  list (APPEND ROMBUNDLER_PLATFORM_SOURCES "${CMAKE_SOURCE_DIR}/src/platforms/static/libretro_compat.c")
  set (ROMBUNDLER_PLATFORM_DEFINES ROMBUNDLER_WASM_DYNAMIC)
else ()
  if (ROMBUNDLER_CORE_LIBRARY)
    list (APPEND ROMBUNDLER_PLATFORM_LIBS "${ROMBUNDLER_CORE_LIBRARY}")
    list (APPEND ROMBUNDLER_PLATFORM_SOURCES "${CMAKE_SOURCE_DIR}/src/platforms/static/libretro_compat.c")
  else ()
    list (APPEND ROMBUNDLER_PLATFORM_SOURCES "${CMAKE_SOURCE_DIR}/src/dummy_core.c")
    set (ROMBUNDLER_PLATFORM_DEFINES ROMBUNDLER_DUMMY_CORE)
  endif ()
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

set (_wasm_link_opts
  "SHELL:-sUSE_GLFW=3"
  "SHELL:-sUSE_WEBGL2=1"
  "SHELL:-sFULL_ES3=1"
  "SHELL:-sALLOW_MEMORY_GROWTH=1"
  "SHELL:-sFORCE_FILESYSTEM=1"
  "SHELL:-sINITIAL_MEMORY=67108864"
  "SHELL:-sEXIT_RUNTIME=0"
  "SHELL:-lopenal"
)

if (ROMBUNDLER_WASM_DYNAMIC)
  list (APPEND _wasm_link_opts
    "SHELL:-sMAIN_MODULE=1"
    "SHELL:-sALLOW_TABLE_GROWTH=1"
    "SHELL:-sINVOKE_RUN=0"
    "SHELL:-sSTACK_SIZE=2097152"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=callMain,FS"
    "SHELL:-sUSE_ZLIB=1"
    "SHELL:--shell-file \"${WASM_SHELL}\""
  )
  target_compile_options (rombundler PRIVATE "SHELL:-sUSE_ZLIB=1")
  message (STATUS "WASM dynamic loader (drop core.wasm + ROM)")
elseif (ROMBUNDLER_WASM_ROM)
  if (NOT EXISTS "${ROMBUNDLER_WASM_ROM}")
    message (FATAL_ERROR "ROMBUNDLER_WASM_ROM not found: ${ROMBUNDLER_WASM_ROM}")
  endif ()
  # Stable VFS name (no spaces) — host path may contain spaces and must be quoted.
  get_filename_component (_wasm_rom_ext "${ROMBUNDLER_WASM_ROM}" EXT)
  if (_wasm_rom_ext STREQUAL "")
    set (_wasm_rom_ext ".bin")
  endif ()
  set (_wasm_rom_vfs "game${_wasm_rom_ext}")
  set (_wasm_cfg "${CMAKE_BINARY_DIR}/wasm_config.ini")
  file (WRITE "${_wasm_cfg}"
"title = ROMBundler
core = bundled
rom = /${_wasm_rom_vfs}
swap_interval = 1
fullscreen = false
hide_cursor = false
map_analog_to_dpad = true
shader = default
filter = nearest
aspect_ratio = 1.333333
window_width = 800
window_height = 600
port0 = 1
")
  # SHELL: + quotes so paths with spaces survive emcc's argv split.
  list (APPEND _wasm_link_opts
    "SHELL:--preload-file \"${_wasm_cfg}@/config.ini\""
    "SHELL:--preload-file \"${ROMBUNDLER_WASM_ROM}@/${_wasm_rom_vfs}\""
  )
  message (STATUS "WASM preload ROM: ${ROMBUNDLER_WASM_ROM} -> /${_wasm_rom_vfs}")
else ()
  list (APPEND _wasm_link_opts
    "SHELL:--preload-file \"${WASM_PRELOAD}/config.ini@/config.ini\""
    "SHELL:--preload-file \"${WASM_PRELOAD}/dummy.bin@/dummy.bin\""
  )
endif ()

target_link_options (rombundler PRIVATE ${_wasm_link_opts})

if (ROMBUNDLER_PLATFORM_DEFINES)
  target_compile_definitions (rombundler PRIVATE ${ROMBUNDLER_PLATFORM_DEFINES})
endif ()
if (ROMBUNDLER_PLATFORM_LIBS)
  target_link_libraries (rombundler PRIVATE ${ROMBUNDLER_PLATFORM_LIBS})
endif ()

set (ROMBUNDLER_INSTALL_PLATFORM "WebAssembly")
if (ROMBUNDLER_WASM_DYNAMIC)
  set (ROMBUNDLER_INSTALL_STEPS [=[Serve the folder over HTTP (browsers block `file://` WASM). Example: `python3 -m http.server -d . 8080` then open `http://localhost:8080/rombundler.html`. Drop a side-module `.wasm` and a ROM, or click the sample button (`dummy_core.wasm` + `dummy.bin`).]=])
  set (ROMBUNDLER_INSTALL_DATA [=[See `wasm.md` in this zip. This build does not bake a core or a ROM. The page writes the dropped files into the Emscripten FS and `dlopen`s the `.wasm`. Build that file with `bash build.sh wasm --side-module core.a` (same Emscripten). SRAM is `/save.srm` for the session.]=])
  set (ROMBUNDLER_INSTALL_LAYOUT [=[Dynamic WASM loader: Emscripten MAIN_MODULE, GLFW3, WebGL2/GLES3, OpenAL. Cores are SIDE_MODULE `.wasm` files dropped on the page.]=])
else ()
  set (ROMBUNDLER_INSTALL_STEPS [=[Serve the folder over HTTP (browsers block `file://` WASM). Example: `python3 -m http.server -d . 8080` then open `http://localhost:8080/rombundler.html`.]=])
  set (ROMBUNDLER_INSTALL_DATA [=[See `wasm.md` in this zip. Default build uses the dummy core and `/dummy.bin` from the preload package. With `--rom game.md`, the ROM is baked into the `.data` file at `/game.md` (host filename may contain spaces). `core=` does not `dlopen` a `.js`/`.wasm` core. SRAM is `/save.srm` in the Emscripten FS (session only unless you add IDBFS).]=])
  set (ROMBUNDLER_INSTALL_LAYOUT [=[Static WASM app: Emscripten GLFW3, WebGL2/GLES3, OpenAL. No runtime `dlopen`.]=])
endif ()

configure_file (
  "${CMAKE_SOURCE_DIR}/cmake/INSTALL.md.in"
  "${CMAKE_BINARY_DIR}/INSTALL.md"
  @ONLY
)

set (WASM_HTML "${CMAKE_BINARY_DIR}/rombundler.html")
set (WASM_JS "${CMAKE_BINARY_DIR}/rombundler.js")
set (WASM_WASM "${CMAKE_BINARY_DIR}/rombundler.wasm")
set (WASM_DATA "${CMAKE_BINARY_DIR}/rombundler.data")
set (WASM_DUMMY_CORE "${CMAKE_BINARY_DIR}/dummy_core.wasm")

set (_wasm_dist_copies
  COMMAND "${CMAKE_COMMAND}" -E copy "${WASM_HTML}" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${WASM_JS}" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${WASM_WASM}" "${STAGE_DIR}/"
)
if (ROMBUNDLER_WASM_DYNAMIC)
  list (APPEND _wasm_dist_copies
    COMMAND "${CMAKE_COMMAND}" -E copy "${WASM_DUMMY_CORE}" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E copy "${WASM_PRELOAD}/dummy.bin" "${STAGE_DIR}/"
  )
else ()
  list (APPEND _wasm_dist_copies
    COMMAND "${CMAKE_COMMAND}" -E copy "${WASM_DATA}" "${STAGE_DIR}/"
  )
endif ()

add_custom_target (rombundler-dist ALL
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${STAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${STAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${DIST_DIR}"
  ${_wasm_dist_copies}
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

if (ROMBUNDLER_WASM_DYNAMIC)
  add_custom_command (
    OUTPUT "${WASM_DUMMY_CORE}"
    COMMAND "${CMAKE_C_COMPILER}" -sSIDE_MODULE=1 -O2
            -I "${CMAKE_SOURCE_DIR}/include"
            -o "${WASM_DUMMY_CORE}"
            "${CMAKE_SOURCE_DIR}/src/dummy_core.c"
    DEPENDS "${CMAKE_SOURCE_DIR}/src/dummy_core.c" "${CMAKE_SOURCE_DIR}/include/libretro.h"
    COMMENT "Linking dummy_core.wasm side module"
    VERBATIM
  )
  add_custom_target (dummy_core_wasm DEPENDS "${WASM_DUMMY_CORE}")
  add_dependencies (rombundler-dist dummy_core_wasm)
  set_property (TARGET rombundler PROPERTY LINK_DEPENDS "${WASM_SHELL}")
endif ()
