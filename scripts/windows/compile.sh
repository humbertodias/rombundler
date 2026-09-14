#!/usr/bin/env bash
# Windows port: MinGW-w64 + static GLFW / OpenAL-Soft (.a).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${ROOT}"

rm -f *.o rombundler rombundler.exe *.dll

ARCH="${ARCH:-x86_64}"
VERSION="${VERSION:-devel}"
GLFW_VERSION="${GLFW_VERSION:-3.4}"
OPENAL_VERSION="${OPENAL_VERSION:-1.24.2}"
DEPS="${ROOT}/.deps/windows-x86_64"
CC="${CC:-x86_64-w64-mingw32-gcc}"
CXX="${CXX:-x86_64-w64-mingw32-g++}"
RC="${RC:-x86_64-w64-mingw32-windres}"

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

cross=(
  -DCMAKE_SYSTEM_NAME=Windows
  -DCMAKE_C_COMPILER="${CC}"
  -DCMAKE_CXX_COMPILER="${CXX}"
  -DCMAKE_RC_COMPILER="${RC}"
  -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32
  -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER
  -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY
  -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY
  -DCMAKE_INSTALL_PREFIX="${DEPS}"
  -DCMAKE_INSTALL_LIBDIR=lib
  -DCMAKE_BUILD_TYPE=Release
)

if [[ ! -f "${DEPS}/lib/libglfw3.a" ]]; then
  fetch_tag "https://github.com/glfw/glfw/archive/refs/tags/${GLFW_VERSION}.tar.gz" \
    "${DEPS}/src/glfw-${GLFW_VERSION}"
  cmake -S "${DEPS}/src/glfw-${GLFW_VERSION}" -B "${DEPS}/build/glfw" \
    -G Ninja \
    "${cross[@]}" \
    -DBUILD_SHARED_LIBS=OFF \
    -DGLFW_BUILD_EXAMPLES=OFF \
    -DGLFW_BUILD_TESTS=OFF \
    -DGLFW_BUILD_DOCS=OFF
  cmake --build "${DEPS}/build/glfw"
  cmake --install "${DEPS}/build/glfw"
fi

if [[ ! -f "${DEPS}/lib/libopenal.a" && ! -f "${DEPS}/lib/libOpenAL32.a" ]]; then
  fetch_tag "https://github.com/kcat/openal-soft/archive/refs/tags/${OPENAL_VERSION}.tar.gz" \
    "${DEPS}/src/openal-soft-${OPENAL_VERSION}"
  cmake -S "${DEPS}/src/openal-soft-${OPENAL_VERSION}" -B "${DEPS}/build/openal" \
    -G Ninja \
    "${cross[@]}" \
    -DLIBTYPE=STATIC \
    -DALSOFT_UTILS=OFF \
    -DALSOFT_EXAMPLES=OFF \
    -DALSOFT_INSTALL_HRTF_DATA=OFF \
    -DALSOFT_INSTALL_AMBDEC_PRESETS=OFF
  cmake --build "${DEPS}/build/openal"
  cmake --install "${DEPS}/build/openal"
fi

if [[ -f "${DEPS}/lib/libOpenAL32.a" && ! -f "${DEPS}/lib/libopenal.a" ]]; then
  ln -sfn libOpenAL32.a "${DEPS}/lib/libopenal.a"
fi

test -f "${DEPS}/lib/libglfw3.a"
test -f "${DEPS}/lib/libopenal.a"

make OS=Windows ARCH="${ARCH}" VERSION="${VERSION}" CC="${CC}" DEPS="${DEPS}" bundle

mkdir -p dist
mv -f ROMBundler-Windows-"${VERSION}"-"${ARCH}".zip "dist/ROMBundler-Windows-${VERSION}-${ARCH}.zip"
test -s "dist/ROMBundler-Windows-${VERSION}-${ARCH}.zip"
