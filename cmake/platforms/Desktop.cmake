# GLFW + OpenAL ports (Linux, Windows, macOS).

if (NOT TARGET rombundler)
  set (ROMBUNDLER_PLATFORM_SOURCES
    "${CMAKE_SOURCE_DIR}/src/platforms/desktop/glad.c"
    "${CMAKE_SOURCE_DIR}/src/platforms/desktop/platform_glfw.c"
    "${CMAKE_SOURCE_DIR}/src/platforms/desktop/audio.c"
    "${CMAKE_SOURCE_DIR}/src/platforms/desktop/input.c"
  )
  set (ROMBUNDLER_PLATFORM_INCLUDES
    "${CMAKE_SOURCE_DIR}/src/platforms/desktop"
  )
  return ()
endif ()

set (_openal_target OpenAL)
if (TARGET OpenAL::OpenAL)
  set (_openal_target OpenAL::OpenAL)
endif ()

if (WIN32)
  target_link_libraries (rombundler PRIVATE glfw ${_openal_target})
  target_compile_definitions (rombundler PRIVATE AL_LIBTYPE_STATIC)
  set_target_properties (rombundler PROPERTIES WIN32_EXECUTABLE TRUE)
  target_link_options (rombundler PRIVATE -static-libgcc -static-libstdc++)
  target_link_libraries (rombundler PRIVATE
    gdi32 user32 shell32 imm32 ole32 uuid winmm ksuser avrt setupapi cfgmgr32 opengl32
  )
  set (ROMBUNDLER_INSTALL_PLATFORM "Windows")
  set (ROMBUNDLER_INSTALL_STEPS [=[Unzip the archive and run `rombundler.exe` from the same folder as `config.ini`.]=])
  set (ROMBUNDLER_INSTALL_DATA [=[Edit `config.ini` so `core` is a libretro `.dll` and `rom` is your ROM. Place both files next to the executable. Optional `options.ini` sets core variables. SRAM is `save.srm` in this folder.]=])
  set (ROMBUNDLER_INSTALL_LAYOUT [=[This is a static Windows build. GLFW and OpenAL-Soft are linked into the executable.]=])
elseif (APPLE)
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
  set (ROMBUNDLER_INSTALL_PLATFORM "macOS")
  set (ROMBUNDLER_INSTALL_STEPS [=[Unzip the archive and run `./rombundler` from the same folder as `config.ini`. If Gatekeeper quarantines the binary: `xattr -cr .`]=])
  set (ROMBUNDLER_INSTALL_DATA [=[Edit `config.ini` so `core` is a libretro `.dylib` and `rom` is your ROM. Place both files next to the executable. Optional `options.ini` sets core variables. SRAM is `save.srm` in this folder.]=])
  set (ROMBUNDLER_INSTALL_LAYOUT [=[This is a static macOS build. GLFW and OpenAL-Soft are linked into the executable.]=])
else ()
  target_link_libraries (rombundler PRIVATE glfw ${_openal_target})
  target_link_libraries (rombundler PRIVATE m dl pthread stdc++)
  set (ROMBUNDLER_INSTALL_PLATFORM "Linux")
  set (ROMBUNDLER_INSTALL_STEPS [=[Unzip the archive and run `./rombundler` from the same folder as `config.ini`.]=])
  set (ROMBUNDLER_INSTALL_DATA [=[Edit `config.ini` so `core` is a libretro `.so` and `rom` is your ROM. Place both files next to the executable. Optional `options.ini` sets core variables. SRAM is `save.srm` in this folder.]=])
  set (ROMBUNDLER_INSTALL_LAYOUT [=[This is a static Linux build. GLFW and OpenAL-Soft are linked into the executable.]=])
endif ()

if (NOT APPLE)
  include (CheckIPOSupported)
  check_ipo_supported (RESULT _ipo)
  if (_ipo)
    set_property (TARGET rombundler PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif ()
endif ()

configure_file (
  "${CMAKE_SOURCE_DIR}/cmake/INSTALL.md.in"
  "${CMAKE_BINARY_DIR}/INSTALL.md"
  @ONLY
)

add_custom_command (TARGET rombundler POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${STAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${STAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${DIST_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:rombundler>" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/src/platforms/desktop/config.ini" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/INSTALL.md" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/README.md" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/COPYING" "${STAGE_DIR}/"
  COMMAND "${CMAKE_COMMAND}"
          -D "STAGE_DIR=${STAGE_DIR}"
          -D "ZIP=${BUNDLE_ZIP}"
          -P "${CMAKE_SOURCE_DIR}/cmake/package_zip.cmake"
  COMMENT "Packaging dist/${BUNDLE_NAME}.zip"
)
