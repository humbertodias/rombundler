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
elseif (WIN32)
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

if (NINTENDO_SWITCH)
  include ("${CMAKE_SOURCE_DIR}/cmake/platforms/NintendoSwitch.cmake")
else ()
  include ("${CMAKE_SOURCE_DIR}/cmake/platforms/Desktop.cmake")
endif ()

add_executable (rombundler ${ROMBUNDLER_SOURCES} ${ROMBUNDLER_PLATFORM_SOURCES})

target_include_directories (rombundler PRIVATE
  ${ROMBUNDLER_PLATFORM_INCLUDES}
  "${CMAKE_SOURCE_DIR}/src"
)
if (NOT NINTENDO_SWITCH)
  target_include_directories (rombundler PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/deps/include"
  )
endif ()

if (NINTENDO_SWITCH)
  set_target_properties (rombundler PROPERTIES LINKER_LANGUAGE CXX)
  target_link_libraries (rombundler PRIVATE ${ROMBUNDLER_PLATFORM_LIBS} EGL glapi drm_nouveau nx stdc++ m)
  if (ROMBUNDLER_PLATFORM_DEFINES)
    target_compile_definitions (rombundler PRIVATE ${ROMBUNDLER_PLATFORM_DEFINES})
  endif ()
elseif (WIN32)
  set (_openal_target OpenAL)
  if (TARGET OpenAL::OpenAL)
    set (_openal_target OpenAL::OpenAL)
  endif ()
  target_link_libraries (rombundler PRIVATE glfw ${_openal_target})
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
  set (_openal_target OpenAL)
  if (TARGET OpenAL::OpenAL)
    set (_openal_target OpenAL::OpenAL)
  endif ()
  target_link_libraries (rombundler PRIVATE glfw ${_openal_target})
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
  set (_openal_target OpenAL)
  if (TARGET OpenAL::OpenAL)
    set (_openal_target OpenAL::OpenAL)
  endif ()
  target_link_libraries (rombundler PRIVATE glfw ${_openal_target})
  target_link_libraries (rombundler PRIVATE m dl pthread stdc++)
endif ()

if (NOT APPLE AND NOT NINTENDO_SWITCH)
  include (CheckIPOSupported)
  check_ipo_supported (RESULT _ipo)
  if (_ipo)
    set_property (TARGET rombundler PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif ()
endif ()

target_compile_options (rombundler PRIVATE -Wall -O3)

set (BUNDLE_NAME "ROMBundler-${ROMBUNDLER_PORT_OS}-${ROMBUNDLER_VERSION}-${ROMBUNDLER_ARCH}")
set (DIST_DIR "${CMAKE_SOURCE_DIR}/dist")
set (STAGE_DIR "${DIST_DIR}/${BUNDLE_NAME}")
set (BUNDLE_ZIP "${DIST_DIR}/${BUNDLE_NAME}.zip")

if (NINTENDO_SWITCH)
  set (ROMBUNDLER_INSTALL_PLATFORM "Nintendo Switch")
  set (ROMBUNDLER_INSTALL_STEPS [=[Copy `rombundler.nro` to `sdmc:/switch/` (Atmosphere / hbmenu) or send it with `nxlink`. Plus+Minus returns to the homebrew menu.]=])
  set (ROMBUNDLER_INSTALL_DATA [=[See `switch.md` in this zip. Copy `config.ini` to `sdmc:/switch/rombundler/config.ini`. Point `rom=` at a file on the SD card. `core=` does not load a RetroArch `*_libnx.nro`. SRAM is `sdmc:/switch/rombundler/save.srm`. Errors: `sdmc:/switch/rombundler/error.log`.]=])
  set (ROMBUNDLER_INSTALL_LAYOUT [=[Static homebrew NRO: Mesa EGL/OpenGL and libnx `audout`. No GLFW/OpenAL. No runtime `dlopen`.]=])
elseif (WIN32)
  set (ROMBUNDLER_INSTALL_PLATFORM "Windows")
  set (ROMBUNDLER_INSTALL_STEPS [=[Unzip the archive and run `rombundler.exe` from the same folder as `config.ini`.]=])
  set (ROMBUNDLER_INSTALL_DATA [=[Edit `config.ini` so `core` is a libretro `.dll` and `rom` is your ROM. Place both files next to the executable. Optional `options.ini` sets core variables. SRAM is `save.srm` in this folder.]=])
  set (ROMBUNDLER_INSTALL_LAYOUT [=[This is a static Windows build. GLFW and OpenAL-Soft are linked into the executable.]=])
elseif (APPLE)
  set (ROMBUNDLER_INSTALL_PLATFORM "macOS")
  set (ROMBUNDLER_INSTALL_STEPS [=[Unzip the archive and run `./rombundler` from the same folder as `config.ini`. If Gatekeeper quarantines the binary: `xattr -cr .`]=])
  set (ROMBUNDLER_INSTALL_DATA [=[Edit `config.ini` so `core` is a libretro `.dylib` and `rom` is your ROM. Place both files next to the executable. Optional `options.ini` sets core variables. SRAM is `save.srm` in this folder.]=])
  set (ROMBUNDLER_INSTALL_LAYOUT [=[This is a static macOS build. GLFW and OpenAL-Soft are linked into the executable.]=])
else ()
  set (ROMBUNDLER_INSTALL_PLATFORM "Linux")
  set (ROMBUNDLER_INSTALL_STEPS [=[Unzip the archive and run `./rombundler` from the same folder as `config.ini`.]=])
  set (ROMBUNDLER_INSTALL_DATA [=[Edit `config.ini` so `core` is a libretro `.so` and `rom` is your ROM. Place both files next to the executable. Optional `options.ini` sets core variables. SRAM is `save.srm` in this folder.]=])
  set (ROMBUNDLER_INSTALL_LAYOUT [=[This is a static Linux build. GLFW and OpenAL-Soft are linked into the executable.]=])
endif ()

configure_file (
  "${CMAKE_SOURCE_DIR}/cmake/INSTALL.md.in"
  "${CMAKE_BINARY_DIR}/INSTALL.md"
  @ONLY
)

if (NINTENDO_SWITCH)
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
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${STAGE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${STAGE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${DIST_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${NX_NRO}" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E copy "${NX_ROMFS}/config.ini" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/INSTALL.md" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/README.md" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/doc/switch.md" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/COPYING" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}"
            -D "STAGE_DIR=${STAGE_DIR}"
            -D "ZIP=${BUNDLE_ZIP}"
            -P "${CMAKE_SOURCE_DIR}/cmake/package_zip.cmake"
    COMMENT "Packaging dist/${BUNDLE_NAME}.zip (NRO)"
  )
else ()
  add_custom_command (TARGET rombundler POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${STAGE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${STAGE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${DIST_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:rombundler>" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/config.ini" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/INSTALL.md" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/README.md" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/COPYING" "${STAGE_DIR}/"
    COMMAND "${CMAKE_COMMAND}"
            -D "STAGE_DIR=${STAGE_DIR}"
            -D "ZIP=${BUNDLE_ZIP}"
            -P "${CMAKE_SOURCE_DIR}/cmake/package_zip.cmake"
    COMMENT "Packaging dist/${BUNDLE_NAME}.zip"
  )
endif ()
