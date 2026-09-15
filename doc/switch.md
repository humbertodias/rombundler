# Nintendo Switch

The Switch port is a single homebrew NRO. There is no `dlopen`: RetroArch `*_libnx.nro` cores cannot be selected from `config.ini`. The default build links a dummy core (color bars). A real core must be a **static** `*_libretro_libnx.a` linked at compile time.

Desktop (Linux, Windows, macOS): [desktop.md](desktop.md). PlayStation Vita: [vita.md](vita.md). WebAssembly: [wasm.md](wasm.md).

## Build

Needs [devkitA64 / libnx](https://devkitpro.org/) plus `switch-mesa`. Docker (linux/amd64 image `rombundler-switch`):

```shell
bash build.sh switch
bash build.sh switch /path/to/genesis_plus_gx_libretro_libnx.a
bash build.sh switch shell
```

`sdk` is an alias for `shell`. The repo is mounted at `/src` in `rombundler-switch`. Cursor / VS Code: Dev Container **Switch (devkitA64)** (`.devcontainer/switch/`).

Local toolchain:

```shell
cmake --preset switch
cmake --build --preset switch
```

With a core: `cmake --preset switch -DROMBUNDLER_CORE_LIBRARY=/path/to/core.a`. Video is Mesa EGL/OpenGL 4.3; audio is libnx `audout` (no GLFW/OpenAL). Zips land in `dist/`. `bash build.sh switch` with no `.a` goes back to the dummy core.

The `.a` may live outside this repo; `build.sh` bind-mounts it into Docker. You can also set `ROMBUNDLER_CORE_LIBRARY`.

## Install and config

Copy `rombundler.nro` to `sdmc:/switch/` (Atmosphere / hbmenu) or send it with `nxlink`. Plus+Minus returns to hbmenu.

Override config on the SD card (this file wins over the copy baked into the NRO):

`sdmc:/switch/rombundler/config.ini`

```ini
title = My Game
core = dummy
rom = /switch/rombundler/game.md
swap_interval = 1
fullscreen = true
hide_cursor = true
map_analog_to_dpad = true
shader = default
filter = nearest
aspect_ratio = 1.333333
port0 = 1
```

Put the ROM next to that config, e.g. `sdmc:/switch/rombundler/game.md`. Do not use `romfs:` unless the file was added under `src/platforms/switch/romfs/` and the NRO was rebuilt. Spaces in the filename are fine; do not wrap the path in quotes.

`core =` does not load a library on Switch. Keep it as `dummy` (or any name **without** `.nro` / `.so`). A dummy NRO with a different `core=` value will error instead of showing color bars.

SRAM is `sdmc:/switch/rombundler/save.srm`. Fatal errors are shown on screen and written to `sdmc:/switch/rombundler/error.log`.

## Static core (Genesis Plus GX)

Build the core as a libnx archive and link it in one step:

```shell
bash build.sh switch --fetch-core genesis_plus_gx
# or: bash build.sh switch --fetch-core genesis
```

That runs `cores.sh` then packages the NRO with `cores/switch/genesis_plus_gx_libretro_libnx.a`. For cartridge ROMs (`.md`) only, `MAKE_FLAGS='HAVE_CHD=0' bash build.sh switch --fetch-core genesis` skips libchdr. Core names come from [`cores.env`](../cores.env) (case-insensitive).

Or pass an existing archive:

```shell
bash build.sh switch cores/switch/genesis_plus_gx_libretro_libnx.a
```

Install the **new** NRO from `dist/`.

The Switch port supplies RetroArch-style `rfseek` / `filestream_*` helpers so CHD-enabled cores can link. Frontend audio symbols are prefixed (`rb_audio_*`) so they do not clash with core internals such as Genesis `audio_init`.
