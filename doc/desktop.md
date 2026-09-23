# Desktop (Linux, Windows, macOS)

Releases are zips with a static `rombundler` (or `rombundler.exe`), `config.ini`, and this note. You do not need to compile to use a [binary release](https://github.com/kivutar/rombundler/releases).

Edit `config.ini` next to the executable:

```ini
title = Shrine Maiden Shizuka Demo 2
core = ./blastem_libretro.dylib
rom = ./Shrine Maiden Shizuka Demo 2.md
swap_interval = 1
full_screen = false
hide_cursor = false
map_analog_to_dpad = true
window_width = 800
window_height = 600
aspect_ratio = 1.333333
```

Cores: [libretro nightly](http://buildbot.libretro.com/nightly/) (respect each core’s license). Windows uses `.dll`, macOS `.dylib`, Linux `.so`. Place the ROM in the same folder and set `rom=`.

The frontend loads the ini, the core, and the ROM. You can rename `rombundler` to the game name, change the icon, and ship the zip.

The sample checked into the repo is `src/platforms/desktop/config.ini` (copied into the desktop dist folder). Nintendo Switch: [switch.md](switch.md). PlayStation Vita: [vita.md](vita.md). WebAssembly: [wasm.md](wasm.md).

## Inputs

```ini
port0 = 3
port1 = 1
```

From `libretro.h`:

```c
#define RETRO_DEVICE_NONE         0
#define RETRO_DEVICE_JOYPAD       1
#define RETRO_DEVICE_MOUSE        2
#define RETRO_DEVICE_KEYBOARD     3
#define RETRO_DEVICE_LIGHTGUN     4
#define RETRO_DEVICE_ANALOG       5
#define RETRO_DEVICE_POINTER      6
```

## Core options

`options.ini` in the same folder:

```ini
fceumm_sndvolume = 7
fceumm_palette = default
fceumm_ntsc_filter = composite
```

## Shaders

```ini
shader = zfast-crt
filter = linear
```

Defaults: `shader = default`, `filter = nearest`. Shaders: `default`, `zfast-crt`, `zfast-lcd`. Use linear filtering with the CRT and LCD shaders.

## Known working cores

lutro, fceumm, blastem, bluemsx, snes9x, genesis_plus_gx, nes, mesen, mesen-s, dosbox_pure, mgba, gambatte, gearsystem, mednafen_psx, pcsx_rearmed, melonds, swanstation, duckstation, fbneo (`*_libretro`).

Not yet compatible: md (input), sameboy (audio), mupen64plus_next / parallel_n64, ppsspp (GL).

## TODO

- Switch full screen / windowed
- Multitap option
