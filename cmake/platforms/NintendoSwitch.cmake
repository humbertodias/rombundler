# Nintendo Switch (libnx). Toolchain: cmake/toolchains/devkita64-libnx.cmake.
# Mesa libglad (OpenGL 4.3 core), not the desktop glad loader.

set (ROMBUNDLER_CORE_LIBRARY "" CACHE FILEPATH "Optional static libretro core (.a) for Switch")

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
rombundler_add_frontend ()

set_target_properties (rombundler PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries (rombundler PRIVATE ${ROMBUNDLER_PLATFORM_LIBS} EGL glapi drm_nouveau nx stdc++ m)
if (ROMBUNDLER_PLATFORM_DEFINES)
  target_compile_definitions (rombundler PRIVATE ${ROMBUNDLER_PLATFORM_DEFINES})
endif ()

set (ROMBUNDLER_INSTALL_PLATFORM "Nintendo Switch")
set (ROMBUNDLER_INSTALL_STEPS [=[Copy `rombundler.nro` to `sdmc:/switch/` (Atmosphere / hbmenu) or send it with `nxlink`. Plus+Minus returns to the homebrew menu.]=])
set (ROMBUNDLER_INSTALL_DATA [=[See `switch.md` in this zip. Copy `config.ini` to `sdmc:/switch/rombundler/config.ini`. Point `rom=` at a file on the SD card. `core=` does not load a RetroArch `*_libnx.nro`. SRAM is `sdmc:/switch/rombundler/save.srm`. Errors: `sdmc:/switch/rombundler/error.log`.]=])
set (ROMBUNDLER_INSTALL_LAYOUT [=[Static homebrew NRO: Mesa EGL/OpenGL and libnx `audout`. No GLFW/OpenAL. No runtime `dlopen`.]=])

find_program (NACPTOOL nacptool REQUIRED)
find_program (ELF2NRO elf2nro REQUIRED)
set (NX_NACP "${CMAKE_BINARY_DIR}/rombundler.nacp")
set (NX_NRO "${CMAKE_BINARY_DIR}/rombundler.nro")
set (NX_ROMFS "${CMAKE_SOURCE_DIR}/src/platforms/switch/romfs")
set (NX_ICON_ARGS)
if (DEFINED DEVKITPRO AND EXISTS "${DEVKITPRO}/libnx/default_icon.jpg")
  list (APPEND NX_ICON_ARGS "--icon=${DEVKITPRO}/libnx/default_icon.jpg")
endif ()

add_custom_command (TARGET rombundler POST_BUILD
  COMMAND "${NACPTOOL}" --create "ROMBundler" "ROMBundler" "${ROMBUNDLER_VERSION}" "${NX_NACP}"
  COMMAND "${ELF2NRO}" "$<TARGET_FILE:rombundler>" "${NX_NRO}"
          --nacp="${NX_NACP}"
          --romfsdir="${NX_ROMFS}"
          ${NX_ICON_ARGS}
  COMMENT "Building rombundler.nro"
  VERBATIM
)

rombundler_package_dist (
  COMMENT "Packaging dist/${BUNDLE_NAME}.zip (NRO)"
  FILES
    "${NX_NRO}"
    "${NX_ROMFS}/config.ini"
    "${CMAKE_SOURCE_DIR}/doc/switch.md"
)
