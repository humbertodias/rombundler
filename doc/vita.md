# PlayStation Vita

The Vita port is a single homebrew VPK. There is no `dlopen`: RetroArch cores cannot be selected from `config.ini`. The default build links a dummy core (moving color bars). A real core must be a **static** `*_libretro.a` (vitasdk) linked at compile time.

Desktop (Linux, Windows, macOS): [desktop.md](desktop.md). Nintendo Switch: [switch.md](switch.md). WebAssembly: [wasm.md](wasm.md).

## Hardware (required)

1. Install **HENkaku / taiHEN** and **VitaShell**.
2. Install **ShaRKBR33D** (VitaDB) so `libshacccg.suprx` is at `ur0:/data/libshacccg.suprx` (about 3.3 MiB). Runtime GLSL will not start without it.
3. Rebuild the Docker image so vitaGL is compiled with the Sony shader compiler (`HAVE_SCE_SHACCCG=1`) and without the spinning vitaGL splash (`NO_SPLASHSCREEN=1`). Do **not** set `SKIP_DOCKER_BUILD` for that rebuild:

```shell
docker build --no-cache -t rombundler-vita -f docker/Dockerfile.vita docker/
rm -rf build-vita
bash build.sh vita
```

4. Copy **`dist/rombundler.vpk`** to the Vita. Do **not** install the `.zip` (that zip only *contains* the VPK). In VitaShell: highlight `rombundler.vpk` → **X** → Install. Confirm the unknown-source prompt. Title ID is **`ROMBUNDLE`** (a new ID; delete `ux0:app/RMBL00001` if it is still there). After install you should already have a bubble named **ROMBundler**. If not: delete `ux0:app/ROMBUNDLE`, VitaShell **△ → Refresh livearea**, then install the VPK again. Fallback: unzip the VPK (it is a zip) into `ux0:app/ROMBUNDLE/` so that folder contains `eboot.bin` and `sce_sys/`, then Refresh livearea (it must say it refreshed at least 1 item). Enable **Settings → HENkaku → Unsafe homebrew**. You can still run `ux0:app/ROMBUNDLE/eboot.bin` from VitaShell. The bubble uses a generic vitasdk sample icon.
5. Dummy build: you should see color bars. START+SELECT (about 0.75 s) returns to LiveArea.
6. If the screen stays black, open `ux0:/data/rombundler/error.log`. `boot: v7-hw` confirms this hardware build. A missing compiler dies with a message about `libshacccg.suprx`.

Override config on `ux0:` (this file wins over the copy inside the VPK):

`ux0:/data/rombundler/config.ini`

```ini
title = My Game
core = dummy
rom = ux0:/data/rombundler/game.md
swap_interval = 1
fullscreen = true
hide_cursor = true
map_analog_to_dpad = true
shader = default
filter = nearest
aspect_ratio = 1.333333
port0 = 1
```

Put the ROM next to that config. Use `app0:` only for files packed into the VPK (`src/platforms/vita/app0/`). Do not wrap paths in quotes.

`core =` does not load a library on Vita. Keep it as `dummy` (or any name **without** `.self` / `.so`). A dummy VPK with a different `core=` value will error instead of showing color bars.

SRAM is `ux0:/data/rombundler/save.srm`. Fatal errors are written to `ux0:/data/rombundler/error.log`.

Video is vitaGL + GLSL ES (`shader=` in `config.ini` is used). Audio is `sceAudioOut` (no GLFW/OpenAL).

## Build

Needs [vitasdk](https://vitasdk.org/) plus **vitaGL** and **vitaShaRK**. Docker (image `rombundler-vita`):

```shell
bash build.sh vita
bash build.sh vita /path/to/genesis_plus_gx_libretro.a
bash build.sh vita shell
```

`sdk` is an alias for `shell`. The repo is mounted at `/src`. Cursor / VS Code: Dev Container **Vita (vitasdk)** (`.devcontainer/vita/`).

Local toolchain (`VITASDK` set, vitaGL installed with `HAVE_SCE_SHACCCG=1`):

```shell
cmake --preset vita
cmake --build --preset vita
```

With a core: `cmake --preset vita -DROMBUNDLER_CORE_LIBRARY=/path/to/core.a`. Zips land in `dist/`. `bash build.sh vita` with no `.a` goes back to the dummy core.

The `.a` may live outside this repo; `build.sh` bind-mounts it into Docker. You can also set `ROMBUNDLER_CORE_LIBRARY`.

## Vita3K

The emulator does not run Sony `libshacccg.suprx`. This hardware GLSL path will typically black-screen or exit there. Use a real Vita for video.

The `dummy.bin` from the VPK is already at `app0:/dummy.bin`. On the host that would be:

`…/Vita3K/fs/ux0/app/ROMBUNDLE/dummy.bin`

## Static core (Genesis Plus GX)

Build the core and VPK in one step:

```shell
bash build.sh vita --fetch-core genesis_plus_gx
# or: bash build.sh vita --fetch-core genesis
```

For cartridge ROMs (`.md`) only, `MAKE_FLAGS='HAVE_CHD=0' bash build.sh vita --fetch-core genesis` skips libchdr. Core names come from [`cores.env`](../cores.env) (case-insensitive).

Or pass an existing archive:

```shell
bash build.sh vita cores/vita/genesis_plus_gx_libretro_vita.a
```

Install the **new** VPK from `dist/`.

The Vita port supplies RetroArch-style `rfseek` / `filestream_*` helpers so CHD-enabled cores can link. Frontend audio symbols are prefixed (`rb_audio_*`) so they do not clash with core internals such as Genesis `audio_init`.
