#!/usr/bin/env bash
# Toolchain image + cmake --preset. Ports stay decoupled (Dockerfile + preset).
#
#   bash build.sh linux
#   bash build.sh windows
#   bash build.sh macos
#   bash build.sh macos arm64
#   bash build.sh switch
#   bash build.sh switch /path/to/core_libretro.a
#   bash build.sh switch --fetch-core genesis_plus_gx
#   bash build.sh switch --fetch-core genesis
#   bash build.sh vita /path/to/core_libretro.a
#   bash build.sh wasm /path/to/core_libretro.a
#   bash build.sh wasm --fetch-core genesis --rom /path/to/game.md
#   bash build.sh switch --core /path/to/core_libretro.a
#   ROMBUNDLER_CORE_LIBRARY=/path/to/core.a bash build.sh switch
#   bash build.sh linux shell
#   bash build.sh switch sdk
#   bash build.sh vita sdk
#   bash build.sh wasm sdk
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "${ROOT}"

# shellcheck disable=SC1091
source "${ROOT}/cores.sh"

USAGE="usage: $0 linux|windows|switch|vita|wasm [shell|sdk] [--core core.a] [--fetch-core name] [--rom file]
       $0 macos [x86_64|arm64] [shell|sdk]
       $0 switch|vita|wasm /path/to/core_libretro.a
       $0 switch|vita|wasm --fetch-core genesis_plus_gx
       $0 wasm --fetch-core genesis --rom game.md
       Core names: see cores.env (case-insensitive)"

set -a
# shellcheck disable=SC1091
source "${ROOT}/versions.env"
set +a

PLATFORM="${1:-linux}"
shift || true
if [[ "${PLATFORM}" == *-* ]]; then
  set -- "${PLATFORM#*-}" "$@"
  PLATFORM="${PLATFORM%%-*}"
fi

case "${PLATFORM}" in
  linux|windows|macos|switch|vita|wasm) ;;
  *)
    echo "${USAGE}" >&2
    exit 1
    ;;
esac

WANT_SHELL=0
MAC_ARCH=""
CORE_LIBRARY="${ROMBUNDLER_CORE_LIBRARY:-}"
FETCH_CORE=""
CORE_NAME="${ROMBUNDLER_CORE_NAME:-dummy}"
WASM_ROM="${ROMBUNDLER_WASM_ROM:-}"

# Label used in dist/ROMBundler-<port>-<core>-<ver>-<arch>.zip
core_label_from() {
  local raw="$1"
  local n
  n="$(basename "${raw}" .git)"
  n="${n%.git}"
  n="${n%.a}"
  n="${n%.bc}"
  n="${n%_libretro*}"
  # Keep filesystem-safe characters only.
  n="$(printf '%s' "${n}" | tr -c 'A-Za-z0-9._+-' '-' | sed -E 's/-+/-/g; s/^-+//; s/-+$//')"
  if [[ -z "${n}" ]]; then
    n="dummy"
  fi
  printf '%s' "${n}"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    shell|sdk)
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
    --fetch-core)
      FETCH_CORE="${2:-}"
      if [[ -z "${FETCH_CORE}" ]]; then
        echo "${USAGE}" >&2
        exit 1
      fi
      shift 2
      ;;
    --fetch-core=*)
      FETCH_CORE="${1#--fetch-core=}"
      shift
      ;;
    --rom)
      WASM_ROM="${2:-}"
      if [[ -z "${WASM_ROM}" ]]; then
        echo "${USAGE}" >&2
        exit 1
      fi
      shift 2
      ;;
    --rom=*)
      WASM_ROM="${1#--rom=}"
      shift
      ;;
    x86_64|amd64|arm64|aarch64|static)
      MAC_ARCH="$1"
      shift
      ;;
    *.a|*.bc)
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

if [[ -n "${FETCH_CORE}" ]]; then
  case "${PLATFORM}" in
    switch|vita|wasm) ;;
    *)
      echo "--fetch-core only works with switch|vita|wasm" >&2
      exit 1
      ;;
  esac
  if [[ -n "${CORE_LIBRARY}" ]]; then
    echo "use either --fetch-core or --core / path.a, not both" >&2
    exit 1
  fi
  if ! core_resolve "${FETCH_CORE}"; then
    echo "unknown core '${FETCH_CORE}' — add it to cores.env or pick a name from:" >&2
    core_list_keys | sed 's/^/  /' >&2
    exit 1
  fi
  FETCH_CORE="${CORE_KEY}"
fi

if [[ -n "${WASM_ROM}" ]]; then
  if [[ "${PLATFORM}" != "wasm" ]]; then
    echo "--rom is only supported for the wasm port" >&2
    exit 1
  fi
  if [[ ! -f "${WASM_ROM}" ]]; then
    echo "ROM not found: ${WASM_ROM}" >&2
    exit 1
  fi
  WASM_ROM="$(cd "$(dirname "${WASM_ROM}")" && pwd)/$(basename "${WASM_ROM}")"
fi

if ! command -v docker >/dev/null 2>&1; then
  echo "Docker is required (https://docs.docker.com/get-docker/)." >&2
  exit 1
fi

IMAGE="rombundler-${PLATFORM}"
DOCKER_PLATFORM=()
if [[ "${PLATFORM}" == "switch" || "${PLATFORM}" == "wasm" ]]; then
  # Host tools / official images are x86_64
  DOCKER_PLATFORM=(--platform linux/amd64)
fi

if [[ "${SKIP_DOCKER_BUILD:-}" != "1" ]]; then
  if [[ "${PLATFORM}" == "macos" ]]; then
    docker build \
      --build-arg MACOSX_SDK="${MACOSX_SDK}" \
      --build-arg MACOSX_SDK_SHA256="${MACOSX_SDK_SHA256}" \
      --build-arg OSX_VERSION_MIN="${OSXCROSS_OSX_VERSION_MIN}" \
      -t "${IMAGE}" \
      -f docker/Dockerfile.macos \
      docker/
  else
    docker build "${DOCKER_PLATFORM[@]}" -t "${IMAGE}" -f "docker/Dockerfile.${PLATFORM}" docker/
  fi
fi

# Build static core first (same image), then link it into the frontend.
if [[ -n "${FETCH_CORE}" ]]; then
  echo "fetch-core: ${FETCH_CORE}"
  CORE_NAME="$(core_label_from "${FETCH_CORE}")"
  SKIP_DOCKER_BUILD=1 bash "${ROOT}/cores.sh" "${PLATFORM}" "${FETCH_CORE}"
  # Prefer real archives; normalize any leftover misnamed *.bc from older builds.
  shopt -s nullglob
  for _bc in "${ROOT}/cores/${PLATFORM}"/*_libretro*.bc; do
    if [[ "$(head -c 7 "${_bc}" 2>/dev/null || true)" == '!<arch>' ]]; then
      _fa="${_bc%.bc}.a"
      cp -f "${_bc}" "${_fa}"
      rm -f "${_bc}"
    fi
  done
  _arts=("${ROOT}/cores/${PLATFORM}"/*_libretro*.a)
  if [[ ${#_arts[@]} -eq 0 ]]; then
    _arts=("${ROOT}/cores/${PLATFORM}"/*_libretro*.bc)
  fi
  shopt -u nullglob
  if [[ ${#_arts[@]} -eq 0 ]]; then
    echo "cores.sh produced no archive under cores/${PLATFORM}/" >&2
    exit 1
  fi
  # Newest mtime wins if several cores were built earlier.
  CORE_LIBRARY="$(ls -t "${_arts[@]}" | head -n1)"
  echo "using core: ${CORE_LIBRARY}"
fi

DOCKER_MOUNTS=(-v "${ROOT}:/src")
CORE_CMAKE_PATH=""
ROM_CMAKE_PATH=""

if [[ -n "${CORE_LIBRARY}" ]]; then
  if [[ ! -f "${CORE_LIBRARY}" ]]; then
    echo "core library not found: ${CORE_LIBRARY}" >&2
    exit 1
  fi
  # Emscripten STATIC_LINKING archives are often misnamed *.bc (ar, not bitcode).
  if [[ "${CORE_LIBRARY}" == *.bc && "$(head -c 7 "${CORE_LIBRARY}" 2>/dev/null || true)" == '!<arch>' ]]; then
    _fixed="${CORE_LIBRARY%.bc}.a"
    cp -f "${CORE_LIBRARY}" "${_fixed}"
    echo "renamed ar-as-bc core to ${_fixed}"
    CORE_LIBRARY="${_fixed}"
  fi
  if [[ -z "${FETCH_CORE}" ]]; then
    CORE_NAME="$(core_label_from "${CORE_LIBRARY}")"
  fi
  CORE_ABS="$(cd "$(dirname "${CORE_LIBRARY}")" && pwd)/$(basename "${CORE_LIBRARY}")"
  if [[ "${CORE_ABS}" == "${ROOT}/"* ]]; then
    CORE_CMAKE_PATH="/src/${CORE_ABS#"${ROOT}"/}"
  else
    CORE_CMAKE_PATH="/core/$(basename "${CORE_ABS}")"
    DOCKER_MOUNTS+=(-v "${CORE_ABS}:${CORE_CMAKE_PATH}:ro")
  fi
fi

if [[ -n "${WASM_ROM}" ]]; then
  if [[ "${WASM_ROM}" == "${ROOT}/"* ]]; then
    ROM_CMAKE_PATH="/src/${WASM_ROM#"${ROOT}"/}"
  else
    ROM_CMAKE_PATH="/rom/$(basename "${WASM_ROM}")"
    DOCKER_MOUNTS+=(-v "${WASM_ROM}:${ROM_CMAKE_PATH}:ro")
  fi
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

if [[ "${WANT_SHELL}" -eq 1 ]]; then
  echo "image: ${IMAGE}"
  echo "preset: ${PRESET}"
  echo "cwd: /src (repo mounted)"
  echo "cmake --preset ${PRESET} && cmake --build --preset ${PRESET}"
  exec docker run --rm -it \
    "${DOCKER_PLATFORM[@]}" \
    "${DOCKER_MOUNTS[@]}" \
    -w /src \
    -e HOME=/tmp \
    "${IMAGE}" bash
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
echo "core-name: ${CORE_NAME}"
if [[ -n "${CORE_CMAKE_PATH}" ]]; then
  echo "core: ${CORE_CMAKE_PATH}"
fi
if [[ -n "${ROM_CMAKE_PATH}" ]]; then
  echo "rom: ${ROM_CMAKE_PATH}"
fi

CMAKE_CORE_ARGS=()
if [[ -n "${CORE_CMAKE_PATH}" ]]; then
  CMAKE_CORE_ARGS=(-DROMBUNDLER_CORE_LIBRARY="${CORE_CMAKE_PATH}")
else
  # Drop a previous core from the CMake cache when building the dummy NRO/VPK/WASM.
  CMAKE_CORE_ARGS=(-DROMBUNDLER_CORE_LIBRARY=)
fi

CMAKE_ROM_ARGS=()
if [[ "${PLATFORM}" == "wasm" ]]; then
  if [[ -n "${ROM_CMAKE_PATH}" ]]; then
    CMAKE_ROM_ARGS=(-DROMBUNDLER_WASM_ROM="${ROM_CMAKE_PATH}")
  else
    CMAKE_ROM_ARGS=(-DROMBUNDLER_WASM_ROM=)
  fi
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
  -DROMBUNDLER_CORE_NAME="${CORE_NAME}" \
  "${CMAKE_CORE_ARGS[@]}" \
  "${CMAKE_ROM_ARGS[@]}"
run_in_image cmake --build --preset "${PRESET}"
