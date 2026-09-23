# Building ROMBundler

Ports live in separate Docker images. Only [Docker](https://www.docker.com/get-started/) is required. Dependency tags are in `versions.env`.

## Dependencies

- **Desktop:** GLFW 3 and OpenAL (fetched by CMake), OpenGL 2.1
- **Switch:** libnx + switch-mesa (OpenGL 4.3 / EGL), libnx `audout`
- **Vita:** vitasdk + vitaGL / GLSL ES, `sceAudioOut`
- **WASM:** Emscripten (GLFW3, WebGL2 / GLES3, OpenAL)

## Docker builds

```shell
bash build.sh linux
bash build.sh windows
bash build.sh macos
bash build.sh macos arm64
bash build.sh switch
bash build.sh switch /path/to/genesis_plus_gx_libretro_libnx.a
bash build.sh switch --fetch-core genesis_plus_gx
bash build.sh switch --fetch-core genesis
bash build.sh vita
bash build.sh vita --fetch-core genesis
bash build.sh wasm
bash build.sh wasm --loader
bash build.sh wasm --side-module core_libretro.a
bash build.sh wasm --fetch-core genesis_plus_gx
bash build.sh wasm --fetch-core genesis --rom game.md
```

`--fetch-core` (switch / vita / wasm only) runs [`cores.sh`](cores.sh) in the same image, then links the resulting `.a` / `.bc` into ROMBundler. Pass a **name** from [`cores.env`](cores.env) (case-insensitive — prefer lowercase). Or pass an existing archive path / `--core` as before. WASM also accepts `--rom` to preload a game into the Emscripten FS (required for Genesis — it will not run on `/dummy.bin`). `bash build.sh wasm --loader` is the GitHub Pages page: it does not bake a core or a ROM. `bash build.sh wasm --side-module core.a` writes `dist/<core>.wasm`, a side module that page can load.

Each command runs `cmake --preset` inside the image (`linux`, `windows`, `macos-x86_64`, `macos-arm64`, `switch`, `vita`, `wasm`). Desktop links GLFW and OpenAL-Soft statically. Switch and Vita have no GLFW/OpenAL. WASM uses Emscripten ports. Output is the folder `dist/ROMBundler-<port>-<core>-<arch>/` (`core` is `dummy` unless you pass `--fetch-core` / a `.a`). The git version is not part of the name, and the build does not write a zip.

### Toolchain shell

Repo mounted at `/src`:

```shell
bash build.sh linux shell
bash build.sh windows shell
bash build.sh macos shell
bash build.sh switch shell
bash build.sh vita shell
bash build.sh wasm shell
```

`sdk` is an alias for `shell`. Then e.g. `cmake --preset switch && cmake --build --preset switch`.

### Dev Containers

Zed, VS Code, and Cursor can use [Dev Containers](https://containers.dev/) (`.devcontainer/`). Default is Linux; pick **Windows (MinGW)**, **macOS (osxcross)**, **Switch (devkitA64)**, **Vita (vitasdk)**, or **WASM (Emscripten)** in the config picker.

## Local CMake

With a host toolchain, CMake 3.24+, and Ninja:

```shell
cmake --preset linux    # or macos on Darwin, wasm with emsdk, etc.
cmake --build --preset linux
```

Static cores (Switch / Vita / WASM):

```shell
cmake --preset switch -DROMBUNDLER_CORE_LIBRARY=/path/to/core.a
cmake --build --preset switch
```

## Static libretro cores (`.a`)

Two steps under the hood (core archive, then frontend link). One command:

```shell
bash build.sh switch --fetch-core genesis_plus_gx
bash build.sh switch --fetch-core genesis
```

Or keep them separate with [`cores.sh`](cores.sh):

```shell
bash cores.sh switch genesis_plus_gx
bash build.sh switch cores/switch/genesis_plus_gx_libretro_libnx.a
```

Names live in [`cores.env`](cores.env) (`NAME=git-url`). Add a line there to register a new core; lookup is case-insensitive (docs use lowercase). Archives land in `cores/<port>/` (gitignored). Optional make knobs: `MAKE_FLAGS='HAVE_CHD=0' bash build.sh switch --fetch-core genesis`.

CI: [`.github/workflows/cores.yml`](.github/workflows/cores.yml) — on **workflow_dispatch**, pick a core from the dropdown (`dummy` skips, `all` builds every name in `cores.env`). Push still smokes `genesis_plus_gx` × switch/vita/wasm. The wasm job also links the chosen core into a droppable side-module `.wasm`.

CD ([`.github/workflows/cd.yml`](.github/workflows/cd.yml)): on **workflow_dispatch**, same core dropdown (`dummy` pre-selected = bundled bars; `all` = one CI matrix entry per `cores.env` name). GitHub **release** still publishes with the dummy core by default, and also builds droppable WASM side modules for `genesis_plus_gx` and `snes9x2010`.

Per-port notes: [doc/desktop.md](doc/desktop.md), [doc/switch.md](doc/switch.md), [doc/vita.md](doc/vita.md), [doc/wasm.md](doc/wasm.md).
