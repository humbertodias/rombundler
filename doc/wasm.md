# WebAssembly

The WASM port is a static Emscripten app (`rombundler.html` + `.js` + `.wasm` + `.data`). There is no `dlopen`: RetroArch cores cannot be selected from `config.ini`. The default build links a dummy core (moving color bars). A real core must be a **static** `*_libretro.a` (Emscripten) linked at compile time.

Desktop (Linux, Windows, macOS): [desktop.md](desktop.md). Nintendo Switch: [switch.md](switch.md). PlayStation Vita: [vita.md](vita.md).

## Run

Browsers block WASM from `file://`. Serve the zip contents over HTTP:

```shell
cd dist/ROMBundler-WASM-*-wasm32   # or unzip the release zip
python3 -m http.server 8080
```

Open `http://localhost:8080/rombundler.html`. You should see color bars (dummy core). Click once if the browser requires a gesture before OpenAL starts.

Preloaded files live at `/config.ini` and `/dummy.bin` inside the Emscripten virtual FS. SRAM is `/save.srm` (in-memory for the session unless you add IDBFS yourself).

## Build

Needs [Emscripten](https://emscripten.org/) (`emcc`). Docker (image `rombundler-wasm`):

```shell
bash build.sh wasm
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

Video is WebGL2 / GLES3 (`shader=` in `config.ini` is used). Audio is OpenAL via Emscripten (non-blocking; drops buffers instead of spinning). Input uses the desktop keyboard map (Z/X/arrows/Enter, etc.). The Emscripten GLFW port does not expose gamepad APIs yet, so USB/Bluetooth pads are not wired in this build.

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

`core =` does not load a library on WASM. Keep it as `dummy` (or any name without `.so` / `.js`). To ship a real ROM, rebuild with `--preload-file` changes under `cmake/platforms/WebAssembly.cmake` (or extend the preload dir) and point `rom=` at that path.

## Static core

Build the core with Emscripten as a static archive, then:

```shell
bash build.sh wasm /path/to/your_libretro.a
```

The `.a` may live outside this repo; `build.sh` bind-mounts it into Docker.
