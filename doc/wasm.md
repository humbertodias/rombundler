# WebAssembly

Two WASM builds:

- **Loader** (`bash build.sh wasm --loader`): the [GitHub Pages](https://humbertodias.github.io/rombundler/) app. It links no core and bakes no ROM. Drop a side-module `.wasm` and a game on the page (or use the sample button). The core must be built with this same Emscripten (`bash build.sh wasm --side-module core.a`). A RetroArch `.wasm`, or a desktop `.so`, will not load.
- **Static** (`bash build.sh wasm`, optional `--fetch-core` / `--rom`): one module with the core linked in. `core=` does not `dlopen`.

Desktop (Linux, Windows, macOS): [desktop.md](desktop.md). Nintendo Switch: [switch.md](switch.md). PlayStation Vita: [vita.md](vita.md).

## Loader page

```shell
bash build.sh wasm --loader
cd dist/ROMBundler-WASM-loader-*-wasm32
python3 -m http.server 8080
```

Open `http://localhost:8080/rombundler.html`. Drop the side module. **Run** enables with only the core; add a ROM when the core needs one. **Sample (color bars)** loads `dummy_core.wasm` and `dummy.bin` from the same folder. Click the picture if the browser blocks audio. SRAM is `/save.srm` for the session.

Turn an Emscripten libretro archive into a file the page can load:

```shell
bash cores.sh wasm genesis_plus_gx
bash build.sh wasm --side-module cores/wasm/genesis_plus_gx_libretro_emscripten.a
```

That writes `dist/genesis_plus_gx_libretro_emscripten.wasm`. Drop it with the game (`.md` for Genesis). The archive has to be the Emscripten one from `cores.sh wasm`, not the Switch/Vita `.a`. `cores.sh wasm` compiles with `-fPIC`. An older `.a` fails at `--side-module` with `recompile with -fPIC`; rebuild it with `bash cores.sh wasm genesis_plus_gx`.

## Static build

The static port is `rombundler.html` + `.js` + `.wasm` + `.data`. There is no `dlopen`: RetroArch cores cannot be selected from `config.ini`. The default build links a dummy core (moving color bars). A real core must be a **static** `*_libretro.a` (Emscripten) linked at compile time.

Browsers block WASM from `file://`. Serve the zip contents over HTTP:

```shell
cd dist/ROMBundler-WASM-dummy-*-wasm32
python3 -m http.server 8080
```

Open `http://localhost:8080/rombundler.html`. You should see color bars (dummy core). Click once if the browser requires a gesture before OpenAL starts.

Preloaded files live at `/config.ini` and `/dummy.bin` inside the Emscripten virtual FS. SRAM is `/save.srm` (in-memory for the session unless you add IDBFS yourself).

## Build

Needs [Emscripten](https://emscripten.org/) (`emcc`). Docker (image `rombundler-wasm`):

```shell
bash build.sh wasm
bash build.sh wasm --loader
bash build.sh wasm /path/to/core_libretro.a
bash build.sh wasm shell
```

`sdk` is an alias for `shell`. The repo is mounted at `/src`. Cursor / VS Code: Dev Container **WASM (Emscripten)** (`.devcontainer/wasm/`).

Local toolchain (`emcc` on `PATH`, or `EMSDK` set):

```shell
cmake --preset wasm
cmake --build --preset wasm
```

With a core: `cmake --preset wasm -DROMBUNDLER_CORE_LIBRARY=/path/to/core.a`. Zips land in `dist/`. `bash build.sh wasm` with no `.a` goes back to the dummy core.

Video is WebGL2 / GLES3 (`shader=` in `config.ini` is used). Audio is OpenAL via Emscripten (non-blocking; drops buffers instead of spinning). Input uses the desktop keyboard map (Z/X/arrows/Enter, etc.) plus browser gamepads through GLFW’s joystick API (`navigator.getGamepads`). Click the canvas once so the browser unlocks the Gamepad API. Mapping assumes the HTML5 Standard Gamepad layout (Xbox-style).

## config.ini

Packed defaults:

```ini
title = ROMBundler
core = dummy
rom = /dummy.bin
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
```

`core =` does not load a library on WASM. Keep it as `dummy` (or any name without `.so` / `.js`).

To ship a real ROM with Genesis (or another static core), bake it in at build time:

```shell
bash build.sh wasm --fetch-core genesis --rom /path/to/game.md
```

That preloads the file at `/game.md` (extension kept; basename sanitized so spaces in the host path do not break `emcc`) and rewrites `/config.ini` so `rom=` points there. Genesis needs a real Mega Drive ROM (`need_fullpath`); `/dummy.bin` alone will not boot a game.

## RGB565 log line

`Frontend supports RGB565 - will use that instead of XRGB1555.` is normal Genesis INFO during `retro_load_game`, not a failure. If the canvas stays black after that, rebuild with the GLES `GL_RGB565` texture fix and a real `--rom`.

## Static core

Build core + WASM package in one step:

```shell
bash build.sh wasm --fetch-core genesis_plus_gx
# or: bash build.sh wasm --fetch-core genesis
# with ROM baked into rombundler.data:
bash build.sh wasm --fetch-core genesis --rom /path/to/game.md
```

Genesis (and other `STATIC_LINKING` cores) may write a misnamed `*_emscripten.bc` that is really an `ar` archive; `cores.sh` / `build.sh` rename it to `.a` before linking. WASM core builds use `emmake` so objects are wasm32, not host ELF. Names are registered in [`cores.env`](../cores.env).

Or pass an existing archive:

```shell
bash build.sh wasm cores/wasm/genesis_plus_gx_libretro_emscripten.a
```

Add or change a name in [`cores.env`](../cores.env) (and the Cores workflow matrix) to build a different core.
