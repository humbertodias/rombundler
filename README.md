[![CI](https://github.com/humbertodias/rombundler/actions/workflows/ci.yml/badge.svg)](https://github.com/humbertodias/rombundler/actions/workflows/ci.yml)
[![CD](https://github.com/humbertodias/rombundler/actions/workflows/cd.yml/badge.svg)](https://github.com/humbertodias/rombundler/actions/workflows/cd.yml)
![GitHub all downloads](https://img.shields.io/github/downloads/humbertodias/rombundler/total)

# ROMBundler

ROMBundler is a way to release your homebrew retro game as an executable.

It is based on this example libretro frontend https://github.com/heuripedes/nanoarch

The frontend is driven by an ini file instead of command-line flags so a core and a ROM can ship as one folder (desktop), one NRO (Switch), or one VPK (Vita). Desktop uses glad and OpenAL; Switch uses Mesa EGL and libnx `audout`; Vita uses vitaGL and `sceAudioOut`.

Usage: [doc/desktop.md](doc/desktop.md) (Linux, Windows, macOS), [doc/switch.md](doc/switch.md) (Nintendo Switch), and [doc/vita.md](doc/vita.md) (PlayStation Vita).

# Compiling

Dependencies:

 * GLFW 3 and OpenAL (Linux, Windows, macOS; fetched by CMake)
 * OpenGL 2.1 (desktop), Mesa OpenGL 4.3 (Switch), or vitaGL / GLSL ES (Vita)
 * libnx + switch-mesa (Switch)
 * vitasdk + vitaGL (Vita)

Dependency tags live in `versions.env`. Ports are built in **separate** Docker images. Only [Docker](https://www.docker.com/get-started/) is required:

```shell
bash build.sh linux
bash build.sh windows
bash build.sh macos
bash build.sh macos arm64
bash build.sh switch
bash build.sh switch /path/to/genesis_plus_gx_libretro_libnx.a
bash build.sh vita
bash build.sh vita /path/to/genesis_plus_gx_libretro.a
```

Each command is `cmake --preset` inside the image (`linux`, `windows`, `macos-x86_64`, `macos-arm64`, `switch`, `vita`). Desktop ports statically link GLFW and OpenAL-Soft. Switch uses libnx + Mesa EGL (no GLFW). Vita uses vitasdk + vitaGL (no GLFW). Zips land in `dist/`.

Interactive toolchain shell (repo at `/src`):

```shell
bash build.sh linux shell
bash build.sh windows shell
bash build.sh macos shell
bash build.sh switch shell
bash build.sh vita shell
```

`sdk` is an alias for `shell`. Then `cmake --preset switch` (or `vita` / `linux` / `windows` / `macos-x86_64`).

Zed, VS Code, and Cursor can attach to the same images via [Dev Containers](https://containers.dev/) (`.devcontainer/`). The default is Linux; pick **Windows (MinGW)**, **macOS (osxcross)**, **Switch (devkitA64)**, or **Vita (vitasdk)** in the config picker.

With a local compiler, CMake 3.24+, and Ninja:

```shell
cmake --preset linux    # or macos on Darwin
cmake --build --preset linux
```
