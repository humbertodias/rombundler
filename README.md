# ROMBundler

ROMBundler is a way to release your homebrew retro game as an executable.

It is based on this example libretro frontend https://github.com/heuripedes/nanoarch

The main differences are that the frontend is controlled by an ini file instead of command line flags to make it easier to bundle ROMs. Also we use glad instead of glew, added openal for cross platform audio, and will add joypad support and maybe a few shaders.

# Compiling

Dependencies are:

 * GLFW 3
 * OpenAL
 * OpenGL 2.1

See our Github Action config files for detailed instructions on how to setup dependencies per OS.

To compile:

```
make
```

# Usage

You don't need to compile `rombundler` if you just want to use it, you can get one of the binary releases [here](https://github.com/kivutar/rombundler/releases)

Then modify the config.ini to fit your needs:

    title = Shrine Maiden Shizuka Demo 2
    core = ./blastem_libretro.dylib
    rom = ./Shrine Maiden Shizuka Demo 2.md
    swap_interval = 1
    full_screen = 0
    scale = 3

You can download libretro cores from http://buildbot.libretro.com/nightly/ and place it in the same folder, as well as your ROM. (But make sure to comply to the core license).

For Windows, the core need to be a `.DLL`, for OSX it needs to be a `.dylib`, and for Linux a `.so`. ROMBundler releases are for 64bit systems only for now.

Place your ROM in the same folder. And set the ROM name in your config.ini.

ROMBundler will read the ini file, load the emulator and the ROM and the game will start.

You can then rename `rombundler` with the name of your game, change it's icon, and distribute this as a zip.

# TODO

 * Static linking for Linux and OSX
 * Switch full screen / windowed
 * Analog input
 * List supported cores
 * Shaders
 * SaveRAM

# Known working cores

 * lutro_libretro
 * fceumm_libretro
 * blastem_libretro
 * snes9x_libretro
 * genesis_plus_gx_libretro
 * nes_libretro
 * mesen_libretro
 * mesen-s_libretro
 * dosbox_pure_libretro
 * mgba_libretro
 * gambatte_libretro
 * gearsystem_libretro

Not yet compatible:

 * Any GL core (not implemented)
 * md_libretro (input issues)
 * sameboy_libretro (audio issues)
 * pcsx_rearmed_libretro (resolution switch not implemented)
