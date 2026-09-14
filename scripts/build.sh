#!/usr/bin/env bash
# Toolchain image + cmake --preset. Ports stay decoupled (Dockerfile + preset).
#
#   bash scripts/build.sh linux
#   bash scripts/build.sh windows
#   bash scripts/build.sh macos
#   bash scripts/build.sh macos arm64
#   bash scripts/build.sh switch
#   bash scripts/build.sh switch /path/to/core_libretro.a
#   bash scripts/build.sh switch --core /path/to/core_libretro.a
#   ROMBUNDLER_CORE_LIBRARY=/path/to/core.a bash scripts/build.sh switch
#   bash scripts/build.sh linux shell
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT}"

USAGE="usage: $0 linux|windows|switch [shell] [--core core.a]
       $0 macos [x86_64|arm64] [shell]
       $0 switch /path/to/core_libretro.a"

if [[ -f "${ROOT}/versions.env" ]]; then
  set -a
  # shellcheck disable=SC1091
  source "${ROOT}/versions.env"
  set +a
fi

PLATFORM="${1:-linux}"
shift || true
if [[ "${PLATFORM}" == *-* ]]; then
  set -- "${PLATFORM#*-}" "$@"
  PLATFORM="${PLATFORM%%-*}"
fi

case "${PLATFORM}" in
  linux|windows|macos|switch) ;;
  *)
    echo "${USAGE}" >&2
    exit 1
    ;;
esac

WANT_SHELL=0
MAC_ARCH=""
CORE_LIBRARY="${ROMBUNDLER_CORE_LIBRARY:-}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    shell)
      WANT_SHELL=1
      shift
      ;;
    --core)
      CORE_LIBRARY="${2:-}"
      if [[ -z "${CORE_LIBRARY}" ]]; then
        echo "${USAGE}" >&2
        exit 1
      fi
      shift 2
      ;;
    --core=*)
      CORE_LIBRARY="${1#--core=}"
      shift
      ;;
    x86_64|amd64|arm64|aarch64|static)
      MAC_ARCH="$1"
      shift
      ;;
    *.a)
      CORE_LIBRARY="$1"
      shift
      ;;
    *)
      if [[ -f "$1" ]]; then
        CORE_LIBRARY="$1"
        shift
      else
        echo "${USAGE}" >&2
        exit 1
      fi
      ;;
  esac
done

if ! command -v docker >/dev/null 2>&1; then
  echo "Docker is required (https://docs.docker.com/get-docker/)." >&2
  exit 1
fi

IMAGE="rombundler-${PLATFORM}"
DOCKER_PLATFORM=()
if [[ "${PLATFORM}" == "switch" ]]; then
  # devkitA64 host tools are x86_64
  DOCKER_PLATFORM=(--platform linux/amd64)
fi

if [[ "${SKIP_DOCKER_BUILD:-}" != "1" ]]; then
  if [[ "${PLATFORM}" == "macos" ]]; then
    docker build \
      --build-arg MACOSX_SDK="${MACOSX_SDK:-15.5}" \
      --build-arg MACOSX_SDK_SHA256="${MACOSX_SDK_SHA256:-c15cf0f3f17d714d1aa5a642da8e118db53d79429eb015771ba816aa7c6c1cbd}" \
      --build-arg OSX_VERSION_MIN="${OSXCROSS_OSX_VERSION_MIN:-11.0}" \
      -t "${IMAGE}" \
      -f docker/Dockerfile.macos \
      docker/
  else
    docker build "${DOCKER_PLATFORM[@]}" -t "${IMAGE}" -f "docker/Dockerfile.${PLATFORM}" docker/
  fi
fi

DOCKER_MOUNTS=(-v "${ROOT}:/src")
CORE_CMAKE_PATH=""

if [[ -n "${CORE_LIBRARY}" ]]; then
  if [[ ! -f "${CORE_LIBRARY}" ]]; then
    echo "core library not found: ${CORE_LIBRARY}" >&2
    exit 1
  fi
  CORE_ABS="$(cd "$(dirname "${CORE_LIBRARY}")" && pwd)/$(basename "${CORE_LIBRARY}")"
  if [[ "${CORE_ABS}" == "${ROOT}/"* ]]; then
    CORE_CMAKE_PATH="/src/${CORE_ABS#"${ROOT}"/}"
  else
    CORE_CMAKE_PATH="/core/$(basename "${CORE_ABS}")"
    DOCKER_MOUNTS+=(-v "${CORE_ABS}:${CORE_CMAKE_PATH}:ro")
  fi
fi

if [[ "${WANT_SHELL}" -eq 1 ]]; then
  exec docker run --rm -it \
    "${DOCKER_PLATFORM[@]}" \
    "${DOCKER_MOUNTS[@]}" \
    -w /src \
    -e HOME=/tmp \
    "${IMAGE}" bash
fi

PRESET="${PLATFORM}"
if [[ "${PLATFORM}" == "macos" ]]; then
  case "${MAC_ARCH}" in
    ""|static|x86_64|amd64) PRESET="macos-x86_64" ;;
    arm64|aarch64) PRESET="macos-arm64" ;;
    *)
      echo "${USAGE}" >&2
      exit 1
      ;;
  esac
elif [[ -n "${MAC_ARCH}" ]]; then
  echo "${USAGE}" >&2
  exit 1
fi

if [[ -z "${VERSION:-}" ]]; then
  VERSION="$(git describe --tags --exact-match HEAD 2>/dev/null \
    || git describe --tags --always --dirty 2>/dev/null \
    || echo devel)"
  VERSION="${VERSION#v}"
fi

echo "image: ${IMAGE}"
echo "preset: ${PRESET}"
echo "version: ${VERSION}"
if [[ -n "${CORE_CMAKE_PATH}" ]]; then
  echo "core: ${CORE_CMAKE_PATH}"
fi

CMAKE_CORE_ARGS=()
if [[ -n "${CORE_CMAKE_PATH}" ]]; then
  CMAKE_CORE_ARGS=(-DROMBUNDLER_CORE_LIBRARY="${CORE_CMAKE_PATH}")
else
  # Drop a previous core from the CMake cache when building the dummy NRO.
  CMAKE_CORE_ARGS=(-DROMBUNDLER_CORE_LIBRARY=)
fi

run_in_image() {
  docker run --rm \
    "${DOCKER_PLATFORM[@]}" \
    -u "$(id -u):$(id -g)" \
    "${DOCKER_MOUNTS[@]}" \
    -w /src \
    -e HOME=/tmp \
    "${IMAGE}" \
    "$@"
}

run_in_image cmake --preset "${PRESET}" \
  -DROMBUNDLER_VERSION="${VERSION}" \
  -DROMBUNDLER_GLFW_VERSION="${GLFW_VERSION:-3.4}" \
  -DROMBUNDLER_OPENAL_VERSION="${OPENAL_VERSION:-1.24.2}" \
  "${CMAKE_CORE_ARGS[@]}"
run_in_image cmake --build --preset "${PRESET}"
