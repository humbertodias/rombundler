# PlayStation Vita (vitasdk). Toolchain: cmake/toolchains/vitasdk.cmake.
# vitaGL (GLES2 / GL compat), not the desktop glad loader.

set (CMAKE_POSITION_INDEPENDENT_CODE OFF)
set (ROMBUNDLER_CORE_LIBRARY "" CACHE FILEPATH "Optional static libretro core (.a) for Vita")

if (DEFINED ENV{VITASDK} AND EXISTS "$ENV{VITASDK}/share/vita.cmake")
  include ("$ENV{VITASDK}/share/vita.cmake")
elseif (DEFINED VITASDK AND EXISTS "${VITASDK}/share/vita.cmake")
  include ("${VITASDK}/share/vita.cmake")
else ()
  message (FATAL_ERROR "vita.cmake not found (set VITASDK)")
endif ()

set (ROMBUNDLER_PLATFORM_SOURCES
  "${CMAKE_SOURCE_DIR}/src/platforms/vita/platform.c"
  "${CMAKE_SOURCE_DIR}/src/platforms/vita/audio.c"
  "${CMAKE_SOURCE_DIR}/src/platforms/vita/input.c"
)
set (ROMBUNDLER_PLATFORM_LIBS)
if (ROMBUNDLER_CORE_LIBRARY)
  list (APPEND ROMBUNDLER_PLATFORM_LIBS "${ROMBUNDLER_CORE_LIBRARY}")
  list (APPEND ROMBUNDLER_PLATFORM_SOURCES "${CMAKE_SOURCE_DIR}/src/platforms/vita/libretro_compat.c")
else ()
  list (APPEND ROMBUNDLER_PLATFORM_SOURCES "${CMAKE_SOURCE_DIR}/src/dummy_core.c")
  set (ROMBUNDLER_PLATFORM_DEFINES ROMBUNDLER_DUMMY_CORE)
endif ()
set (ROMBUNDLER_PLATFORM_INCLUDES
  "${CMAKE_SOURCE_DIR}/src/platforms/vita"
)
list (APPEND ROMBUNDLER_PLATFORM_DEFINES VITA)
rombundler_add_frontend ()

set_target_properties (rombundler PROPERTIES
  LINKER_LANGUAGE CXX
  POSITION_INDEPENDENT_CODE OFF
)
target_link_libraries (rombundler PRIVATE
  ${ROMBUNDLER_PLATFORM_LIBS}
  vitaGL
  vitashark
  SceShaccCgExt
  taihen_stub
  mathneon
  SceShaccCg_stub
  SceAppMgr_stub
  SceAppUtil_stub
  SceAudio_stub
  SceCommonDialog_stub
  SceCtrl_stub
  SceDisplay_stub
  SceGxm_stub
  SceIme_stub
  SceKernelDmacMgr_stub
  SceLibKernel_stub
  ScePvf_stub
  SceSysmodule_stub
  SceTouch_stub
  stdc++
  m
)
if (ROMBUNDLER_PLATFORM_DEFINES)
  target_compile_definitions (rombundler PRIVATE ${ROMBUNDLER_PLATFORM_DEFINES})
endif ()

set (ROMBUNDLER_INSTALL_PLATFORM "PlayStation Vita")
set (ROMBUNDLER_INSTALL_STEPS [=[Install ShaRKBR33D so `ur0:/data/libshacccg.suprx` exists. Copy `rombundler.vpk` (not the `.zip`) to the Vita. In VitaShell highlight the VPK and press X to install. Title ID is `ROMBUNDLE`. START+SELECT returns to LiveArea.]=])
set (ROMBUNDLER_INSTALL_DATA [=[See `vita.md` in this zip. Copy `config.ini` to `ux0:/data/rombundler/config.ini`. Point `rom=` at a file on `ux0:`. `core=` does not load a RetroArch `.self`. SRAM is `ux0:/data/rombundler/save.srm`. Errors: `ux0:/data/rombundler/error.log`.]=])
set (ROMBUNDLER_INSTALL_LAYOUT [=[Static homebrew VPK: vitaGL GLSL (libshacccg) and `sceAudioOut`. No GLFW/OpenAL. No runtime `dlopen`.]=])

set (VITA_APP0 "${CMAKE_SOURCE_DIR}/src/platforms/vita/app0")
set (VITA_SCE_SYS "${CMAKE_SOURCE_DIR}/src/platforms/vita/sce_sys")
set (VITA_VPK "${CMAKE_BINARY_DIR}/rombundler.vpk")
# CATEGORY=gd + ATTRIBUTE 0x8000 are required for a LiveArea bubble.
# PARENTAL_LEVEL=0 so the bubble is not hidden. ATTRIBUTE2=12 is extra RAM for vitaGL.
# TITLE_ID ROMBUNDLE avoids a poisoned app.db row from earlier RMBL00001 installs.
set (VITA_TITLEID "ROMBUNDLE")
set (VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d PARENTAL_LEVEL=0 -d ATTRIBUTE=32768 -d ATTRIBUTE2=12 -s CATEGORY=gd -s BOOT_FILE=eboot.bin")

if (ROMBUNDLER_VERSION MATCHES "^[0-9][0-9]\\.[0-9][0-9]$")
  set (_vita_app_ver "${ROMBUNDLER_VERSION}")
else ()
  set (_vita_app_ver "01.00")
endif ()

foreach (_vita_livearea
    "${VITA_SCE_SYS}/icon0.png"
    "${VITA_SCE_SYS}/livearea/contents/bg.png"
    "${VITA_SCE_SYS}/livearea/contents/startup.png"
    "${VITA_SCE_SYS}/livearea/contents/template.xml")
  if (NOT EXISTS "${_vita_livearea}")
    message (FATAL_ERROR "Missing LiveArea file: ${_vita_livearea}")
  endif ()
endforeach ()

vita_create_self (eboot.bin rombundler UNSAFE)
vita_create_vpk (rombundler.vpk ${VITA_TITLEID} eboot.bin
  VERSION "${_vita_app_ver}"
  NAME "ROMBundler"
  FILE
    "${VITA_APP0}/config.ini" config.ini
    "${VITA_APP0}/dummy.bin" dummy.bin
    "${VITA_SCE_SYS}/icon0.png" sce_sys/icon0.png
    "${VITA_SCE_SYS}/livearea/contents/bg.png" sce_sys/livearea/contents/bg.png
    "${VITA_SCE_SYS}/livearea/contents/startup.png" sce_sys/livearea/contents/startup.png
    "${VITA_SCE_SYS}/livearea/contents/template.xml" sce_sys/livearea/contents/template.xml
)

configure_file (
  "${CMAKE_SOURCE_DIR}/cmake/INSTALL.md.in"
  "${CMAKE_BINARY_DIR}/INSTALL.md"
  @ONLY
)

add_custom_target (rombundler-dist ALL
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${STAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${STAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${DIST_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy "${VITA_VPK}" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${VITA_VPK}" "${DIST_DIR}/rombundler.vpk"
  COMMAND "${CMAKE_COMMAND}" -E copy "${VITA_APP0}/config.ini" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/INSTALL.md" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/README.md" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/doc/vita.md" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/COPYING" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}"
          -D "STAGE_DIR=${STAGE_DIR}"
          -D "ZIP=${BUNDLE_ZIP}"
          -P "${CMAKE_SOURCE_DIR}/cmake/package_zip.cmake"
  COMMENT "Packaging dist/${BUNDLE_NAME}.zip (VPK)"
  VERBATIM
)
if (TARGET rombundler.vpk-vpk)
  add_dependencies (rombundler-dist rombundler.vpk-vpk)
elseif (TARGET rombundler.vpk)
  add_dependencies (rombundler-dist rombundler.vpk)
endif ()
