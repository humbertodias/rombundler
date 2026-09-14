#!/usr/bin/env bash
# Linux port: static GLFW + OpenAL-Soft (.a), then host gcc.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${ROOT}"

rm -f *.o rombundler rombundler.exe *.dll

ARCH="${ARCH:-$(uname -m)}"
VERSION="${VERSION:-devel}"
GLFW_VERSION="${GLFW_VERSION:-3.4}"
OPENAL_VERSION="${OPENAL_VERSION:-1.24.2}"
DEPS="${ROOT}/.deps/linux-${ARCH}"

mkdir -p "${DEPS}/src"

fetch_tag() {
  local url="$1" dest="$2"
  if [[ -d "${dest}" ]]; then
    return 0
  fi
  curl -fsSL -o /tmp/dep.tar.gz "${url}"
  tar -xzf /tmp/dep.tar.gz -C "$(dirname "${dest}")"
  rm -f /tmp/dep.tar.gz
}

if [[ ! -f "${DEPS}/lib/libglfw3.a" ]]; then
  fetch_tag "https://github.com/glfw/glfw/archive/refs/tags/${GLFW_VERSION}.tar.gz" \
    "${DEPS}/src/glfw-${GLFW_VERSION}"
  cmake -S "${DEPS}/src/glfw-${GLFW_VERSION}" -B "${DEPS}/build/glfw" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${DEPS}" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DGLFW_BUILD_EXAMPLES=OFF \
    -DGLFW_BUILD_TESTS=OFF \
    -DGLFW_BUILD_DOCS=OFF \
    -DGLFW_BUILD_WAYLAND=OFF
  cmake --build "${DEPS}/build/glfw"
  cmake --install "${DEPS}/build/glfw"
fi

if [[ ! -f "${DEPS}/lib/libopenal.a" ]]; then
  fetch_tag "https://github.com/kcat/openal-soft/archive/refs/tags/${OPENAL_VERSION}.tar.gz" \
    "${DEPS}/src/openal-soft-${OPENAL_VERSION}"
  cmake -S "${DEPS}/src/openal-soft-${OPENAL_VERSION}" -B "${DEPS}/build/openal" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${DEPS}" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DLIBTYPE=STATIC \
    -DALSOFT_UTILS=OFF \
    -DALSOFT_EXAMPLES=OFF \
    -DALSOFT_INSTALL_HRTF_DATA=OFF \
    -DALSOFT_INSTALL_AMBDEC_PRESETS=OFF
  cmake --build "${DEPS}/build/openal"
  cmake --install "${DEPS}/build/openal"
fi

test -f "${DEPS}/lib/libglfw3.a"
test -f "${DEPS}/lib/libopenal.a"

make OS=Linux ARCH="${ARCH}" VERSION="${VERSION}" DEPS="${DEPS}" bundle

mkdir -p dist
mv -f ROMBundler-Linux-"${VERSION}"-"${ARCH}".zip "dist/ROMBundler-Linux-${VERSION}-${ARCH}.zip"
test -s "dist/ROMBundler-Linux-${VERSION}-${ARCH}.zip"
