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
bash build.sh switch --fetch-core genesis
bash build.sh switch --fetch-core https://github.com/libretro/Genesis-Plus-GX.git
bash build.sh vita
bash build.sh vita --fetch-core genesis
bash build.sh wasm
bash build.sh wasm --fetch-core genesis
```

`--fetch-core` (switch / vita / wasm only) runs [`cores.sh`](cores.sh) in the same image, then links the resulting `.a` / `.bc` into ROMBundler. Pass a git URL or a short alias (`genesis`). Or pass an existing archive path / `--core` as before.

Each command runs `cmake --preset` inside the image (`linux`, `windows`, `macos-x86_64`, `macos-arm64`, `switch`, `vita`, `wasm`). Desktop links GLFW and OpenAL-Soft statically. Switch and Vita have no GLFW/OpenAL. WASM uses Emscripten ports. Zips land in `dist/` as `ROMBundler-<port>-<core>-<version>-<arch>.zip` (`core` is `dummy` unless you pass `--fetch-core` / a `.a`).

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
bash build.sh switch --fetch-core https://github.com/libretro/Genesis-Plus-GX.git
bash build.sh switch --fetch-core genesis
```

Or keep them separate with [`cores.sh`](cores.sh):

```shell
bash cores.sh switch https://github.com/libretro/Genesis-Plus-GX.git
bash build.sh switch cores/switch/genesis_plus_gx_libretro_libnx.a
```

Archives land in `cores/<port>/` (gitignored). Optional make knobs: `MAKE_FLAGS='HAVE_CHD=0' bash build.sh switch --fetch-core genesis`.

CI: [`.github/workflows/cores.yml`](.github/workflows/cores.yml) builds `matrix.core_url` × switch/vita/wasm. Add more URLs under `core_url:` in that file.

CD ([`.github/workflows/cd.yml`](.github/workflows/cd.yml)): on **workflow_dispatch**, optional input `core_url` runs `--fetch-core` for switch/vita/wasm (desktop stays unchanged). Leave empty for the dummy core. GitHub **release** still publishes with the dummy core by default.

Per-port notes: [doc/desktop.md](doc/desktop.md), [doc/switch.md](doc/switch.md), [doc/vita.md](doc/vita.md), [doc/wasm.md](doc/wasm.md).
