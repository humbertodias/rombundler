# GLFW + OpenAL ports (Linux, Windows, macOS).

include ("${CMAKE_SOURCE_DIR}/cmake/FetchDeps.cmake")

set (ROMBUNDLER_PLATFORM_SOURCES
  "${CMAKE_SOURCE_DIR}/src/platforms/desktop/glad.c"
  "${CMAKE_SOURCE_DIR}/src/platforms/desktop/platform.c"
  "${CMAKE_SOURCE_DIR}/src/platforms/desktop/audio.c"
  "${CMAKE_SOURCE_DIR}/src/platforms/desktop/input.c"
)
set (ROMBUNDLER_PLATFORM_INCLUDES
  "${CMAKE_SOURCE_DIR}/src/platforms/desktop"
)
rombundler_add_frontend ()

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
  set (ROMBUNDLER_INSTALL_DATA [=[See `desktop.md` in this folder. Edit `config.ini` so `core` is a libretro `.dll` and `rom` is your ROM. Place both files next to the executable. SRAM is `save.srm` in this folder.]=])
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
  set (ROMBUNDLER_INSTALL_DATA [=[See `desktop.md` in this folder. Edit `config.ini` so `core` is a libretro `.dylib` and `rom` is your ROM. Place both files next to the executable. SRAM is `save.srm` in this folder.]=])
  set (ROMBUNDLER_INSTALL_LAYOUT [=[This is a static macOS build. GLFW and OpenAL-Soft are linked into the executable.]=])
else ()
  target_link_libraries (rombundler PRIVATE glfw ${_openal_target})
  target_link_libraries (rombundler PRIVATE m dl pthread stdc++)
  set (ROMBUNDLER_INSTALL_PLATFORM "Linux")
  set (ROMBUNDLER_INSTALL_STEPS [=[Unzip the archive and run `./rombundler` from the same folder as `config.ini`.]=])
  set (ROMBUNDLER_INSTALL_DATA [=[See `desktop.md` in this folder. Edit `config.ini` so `core` is a libretro `.so` and `rom` is your ROM. Place both files next to the executable. SRAM is `save.srm` in this folder.]=])
  set (ROMBUNDLER_INSTALL_LAYOUT [=[This is a static Linux build. GLFW and OpenAL-Soft are linked into the executable.]=])
endif ()

if (NOT APPLE)
  include (CheckIPOSupported)
  check_ipo_supported (RESULT _ipo)
  if (_ipo)
    set_property (TARGET rombundler PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif ()
endif ()

rombundler_package_dist (FILES
  "$<TARGET_FILE:rombundler>"
  "${CMAKE_SOURCE_DIR}/src/platforms/desktop/config.ini"
  "${CMAKE_SOURCE_DIR}/doc/desktop.md"
)
