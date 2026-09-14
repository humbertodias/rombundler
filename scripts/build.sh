#!/usr/bin/env bash
# Toolchain image + cmake --preset. Ports stay decoupled (Dockerfile + preset).
#
#   bash scripts/build.sh linux
#   bash scripts/build.sh windows
#   bash scripts/build.sh macos
#   bash scripts/build.sh macos arm64
#   bash scripts/build.sh linux shell
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT}"

USAGE="usage: $0 linux|windows [shell]
       $0 macos [x86_64|arm64] [shell]"

if [[ -f "${ROOT}/versions.env" ]]; then
  set -a
  # shellcheck disable=SC1091
  source "${ROOT}/versions.env"
  set +a
fi

PLATFORM="${1:-linux}"
SECOND="${2:-}"
if [[ "${PLATFORM}" == *-* && -z "${SECOND}" ]]; then
  SECOND="${PLATFORM#*-}"
  PLATFORM="${PLATFORM%%-*}"
fi

case "${PLATFORM}" in
  linux|windows|macos) ;;
  *)
    echo "${USAGE}" >&2
    exit 1
    ;;
esac

if ! command -v docker >/dev/null 2>&1; then
  echo "Docker is required (https://docs.docker.com/get-docker/)." >&2
  exit 1
fi

IMAGE="rombundler-${PLATFORM}"

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
    docker build -t "${IMAGE}" -f "docker/Dockerfile.${PLATFORM}" docker/
  fi
fi

if [[ "${SECOND}" == "shell" ]]; then
  exec docker run --rm -it \
    -v "${ROOT}:/src" \
    -w /src \
    -e HOME=/tmp \
    "${IMAGE}" bash
fi

PRESET="${PLATFORM}"
if [[ "${PLATFORM}" == "macos" ]]; then
  case "${SECOND}" in
    ""|static|x86_64|amd64) PRESET="macos-x86_64" ;;
    arm64|aarch64) PRESET="macos-arm64" ;;
    *)
      echo "${USAGE}" >&2
      exit 1
      ;;
  esac
elif [[ -n "${SECOND}" ]]; then
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

docker run --rm \
  -u "$(id -u):$(id -g)" \
  -v "${ROOT}:/src" \
  -w /src \
  -e HOME=/tmp \
  "${IMAGE}" \
  cmake --preset "${PRESET}" \
    -DROMBUNDLER_VERSION="${VERSION}" \
    -DROMBUNDLER_GLFW_VERSION="${GLFW_VERSION:-3.4}" \
    -DROMBUNDLER_OPENAL_VERSION="${OPENAL_VERSION:-1.24.2}" \
  && docker run --rm \
  -u "$(id -u):$(id -g)" \
  -v "${ROOT}:/src" \
  -w /src \
  -e HOME=/tmp \
  "${IMAGE}" \
  cmake --build --preset "${PRESET}"
