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
bash build.sh vita
bash build.sh vita /path/to/genesis_plus_gx_libretro.a
bash build.sh wasm
bash build.sh wasm /path/to/core_libretro.a
```

Each command runs `cmake --preset` inside the image (`linux`, `windows`, `macos-x86_64`, `macos-arm64`, `switch`, `vita`, `wasm`). Desktop links GLFW and OpenAL-Soft statically. Switch and Vita have no GLFW/OpenAL. WASM uses Emscripten ports. Zips land in `dist/`.

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

Per-port notes: [doc/desktop.md](doc/desktop.md), [doc/switch.md](doc/switch.md), [doc/vita.md](doc/vita.md), [doc/wasm.md](doc/wasm.md).
