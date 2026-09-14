#!/usr/bin/env bash
# macOS port: osxcross (Linux) or native clang (Darwin). Static GLFW + OpenAL-Soft (.a).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${ROOT}"

rm -f *.o rombundler rombundler.exe *.dll

ARCH="${MACOS_ARCH:-${ARCH:-x86_64}}"
case "${ARCH}" in
  aarch64) ARCH=arm64 ;;
esac
VERSION="${VERSION:-devel}"
GLFW_VERSION="${GLFW_VERSION:-3.4}"
OPENAL_VERSION="${OPENAL_VERSION:-1.24.2}"
DEPS="${ROOT}/.deps/macos-${ARCH}"

if command -v osxcross-conf >/dev/null 2>&1; then
  eval "$(osxcross-conf)"
fi

if [[ "${ARCH}" == "arm64" ]] && command -v oa64-clang >/dev/null 2>&1; then
  CC=oa64-clang
  CXX=oa64-clang++
elif [[ "${ARCH}" == "x86_64" ]] && command -v o64-clang >/dev/null 2>&1; then
  CC=o64-clang
  CXX=o64-clang++
else
  CC="${CC:-cc}"
  CXX="${CXX:-c++}"
fi

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

cmake_args=(
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_INSTALL_PREFIX="${DEPS}"
  -DCMAKE_INSTALL_LIBDIR=lib
  -DCMAKE_C_COMPILER="${CC}"
  -DCMAKE_CXX_COMPILER="${CXX}"
)
if command -v osxcross-conf >/dev/null 2>&1 || [[ "${CC}" == *apple-darwin* || "${CC}" == o64-* || "${CC}" == oa64-* ]]; then
  if [[ "${ARCH}" == "arm64" ]]; then
    OSX_AR="$(echo /opt/osxcross/target/bin/aarch64-apple-darwin*-ar | awk '{print $1}')"
  else
    OSX_AR="$(echo /opt/osxcross/target/bin/x86_64-apple-darwin*-ar | awk '{print $1}')"
  fi
  OSX_RANLIB="${OSX_AR%-ar}-ranlib"
  cmake_args+=(
    -DCMAKE_SYSTEM_NAME=Darwin
    -DCMAKE_OSX_ARCHITECTURES="${ARCH}"
    -DCMAKE_AR="${OSX_AR}"
    -DCMAKE_RANLIB="${OSX_RANLIB}"
  )
  if [[ -n "${SDKROOT:-}" ]]; then
    cmake_args+=(-DCMAKE_OSX_SYSROOT="${SDKROOT}")
  fi
fi

if [[ ! -f "${DEPS}/lib/libglfw3.a" ]]; then
  fetch_tag "https://github.com/glfw/glfw/archive/refs/tags/${GLFW_VERSION}.tar.gz" \
    "${DEPS}/src/glfw-${GLFW_VERSION}"
  cmake -S "${DEPS}/src/glfw-${GLFW_VERSION}" -B "${DEPS}/build/glfw" \
    "${cmake_args[@]}" \
    -DBUILD_SHARED_LIBS=OFF \
    -DGLFW_BUILD_EXAMPLES=OFF \
    -DGLFW_BUILD_TESTS=OFF \
    -DGLFW_BUILD_DOCS=OFF
  cmake --build "${DEPS}/build/glfw" --parallel
  cmake --install "${DEPS}/build/glfw"
fi

if [[ ! -f "${DEPS}/lib/libopenal.a" ]]; then
  fetch_tag "https://github.com/kcat/openal-soft/archive/refs/tags/${OPENAL_VERSION}.tar.gz" \
    "${DEPS}/src/openal-soft-${OPENAL_VERSION}"
  cmake -S "${DEPS}/src/openal-soft-${OPENAL_VERSION}" -B "${DEPS}/build/openal" \
    "${cmake_args[@]}" \
    -DLIBTYPE=STATIC \
    -DALSOFT_UTILS=OFF \
    -DALSOFT_EXAMPLES=OFF \
    -DALSOFT_INSTALL_HRTF_DATA=OFF \
    -DALSOFT_INSTALL_AMBDEC_PRESETS=OFF
  cmake --build "${DEPS}/build/openal" --parallel
  cmake --install "${DEPS}/build/openal"
fi

test -f "${DEPS}/lib/libglfw3.a"
test -f "${DEPS}/lib/libopenal.a"

make OS=OSX ARCH="${ARCH}" VERSION="${VERSION}" CC="${CC}" DEPS="${DEPS}" bundle

BIN="${ROOT}/rombundler"
if command -v osxcross-codesign >/dev/null 2>&1 && [[ -f "${BIN}" ]]; then
  osxcross-codesign -s - -f "${BIN}" || true
elif command -v codesign >/dev/null 2>&1 && [[ -f "${BIN}" ]]; then
  codesign --force --sign - --timestamp=none "${BIN}" || true
fi

mkdir -p dist
mv -f ROMBundler-OSX-"${VERSION}"-"${ARCH}".zip "dist/ROMBundler-OSX-${VERSION}-${ARCH}.zip"
test -s "dist/ROMBundler-OSX-${VERSION}-${ARCH}.zip"
