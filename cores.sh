#!/usr/bin/env bash
# Build a static libretro core inside a port Docker image.
#
#   bash cores.sh switch https://github.com/libretro/Genesis-Plus-GX.git
#   bash cores.sh vita https://github.com/libretro/Genesis-Plus-GX.git
#   bash cores.sh wasm https://github.com/libretro/Genesis-Plus-GX.git
#
# Change the git URL to build a different core (must ship Makefile.libretro).
# Short alias: bash cores.sh switch genesis
#
# Output: cores/<port>/*_libretro*.{a,bc}
# Optional: MAKE_FLAGS="HAVE_CHD=0" bash cores.sh switch <url>
# Optional: SKIP_DOCKER_BUILD=1 (reuse existing rombundler-<port> image)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "${ROOT}"

USAGE="usage: $0 switch|vita|wasm <git-url|alias>
examples:
  $0 switch https://github.com/libretro/Genesis-Plus-GX.git
  $0 vita genesis
  MAKE_FLAGS='HAVE_CHD=0' $0 switch genesis"

PORT="${1:-}"
RAW="${2:-}"
if [[ -z "${PORT}" || -z "${RAW}" ]]; then
  echo "${USAGE}" >&2
  exit 1
fi

case "${PORT}" in
  switch) MAKE_PLATFORM=libnx ;;
  vita) MAKE_PLATFORM=vita ;;
  wasm) MAKE_PLATFORM=emscripten ;;
  *)
    echo "${USAGE}" >&2
    exit 1
    ;;
esac

# Optional short names → clone URL. Anything else is used as the git URL.
case "${RAW}" in
  genesis|Genesis-Plus-GX|genesis_plus_gx)
    CORE_URL="https://github.com/libretro/Genesis-Plus-GX.git"
    ;;
  http://*|https://*|git@*|ssh://*)
    CORE_URL="${RAW}"
    ;;
  *)
    # Bare "org/repo" → GitHub
    if [[ "${RAW}" == */* && "${RAW}" != */*/* ]]; then
      CORE_URL="https://github.com/${RAW}.git"
    else
      CORE_URL="${RAW}"
    fi
    ;;
esac

CORE_SLUG="$(basename "${CORE_URL}" .git)"
CORE_SLUG="${CORE_SLUG%.git}"
BUILD_DIR="cores/build/${PORT}/${CORE_SLUG}"
OUT_DIR="cores/${PORT}"

if ! command -v docker >/dev/null 2>&1; then
  echo "Docker is required (https://docs.docker.com/get-docker/)." >&2
  exit 1
fi

IMAGE="rombundler-${PORT}"
DOCKER_PLATFORM=()
if [[ "${PORT}" == "switch" || "${PORT}" == "wasm" ]]; then
  DOCKER_PLATFORM=(--platform linux/amd64)
fi

if [[ "${SKIP_DOCKER_BUILD:-}" != "1" ]]; then
  docker build "${DOCKER_PLATFORM[@]}" -t "${IMAGE}" -f "docker/Dockerfile.${PORT}" docker/
fi

echo "image: ${IMAGE}"
echo "port: ${PORT} (make platform=${MAKE_PLATFORM})"
echo "url: ${CORE_URL}"
echo "out: ${OUT_DIR}/"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}" "${OUT_DIR}"

# Clone on the host (auth/network), build inside the toolchain image.
git clone --depth 1 "${CORE_URL}" "${BUILD_DIR}"

if [[ ! -f "${BUILD_DIR}/Makefile.libretro" ]]; then
  echo "no Makefile.libretro in ${CORE_URL}" >&2
  exit 1
fi

MAKE_FLAGS="${MAKE_FLAGS:-}"
# shellcheck disable=SC2086
docker run --rm \
  "${DOCKER_PLATFORM[@]}" \
  -u "$(id -u):$(id -g)" \
  -v "${ROOT}:/src" \
  -w "/src/${BUILD_DIR}" \
  -e HOME=/tmp \
  "${IMAGE}" \
  bash -lc "make -f Makefile.libretro platform=${MAKE_PLATFORM} -j\"\$(nproc)\" ${MAKE_FLAGS}"

shopt -s nullglob
ARTIFACTS=("${BUILD_DIR}"/*_libretro*.a "${BUILD_DIR}"/*_libretro*.bc)
shopt -u nullglob
if [[ ${#ARTIFACTS[@]} -eq 0 ]]; then
  # Some trees write the archive one level down.
  shopt -s nullglob
  ARTIFACTS=("${BUILD_DIR}"/*/*_libretro*.a "${BUILD_DIR}"/*/*_libretro*.bc)
  shopt -u nullglob
fi
if [[ ${#ARTIFACTS[@]} -eq 0 ]]; then
  echo "make finished but no *_libretro*.a / *.bc found under ${BUILD_DIR}" >&2
  exit 1
fi

for art in "${ARTIFACTS[@]}"; do
  cp -f "${art}" "${OUT_DIR}/"
  echo "built: ${OUT_DIR}/$(basename "${art}")"
done

rm -rf "${BUILD_DIR}"
echo "done. link with: bash build.sh ${PORT} ${OUT_DIR}/$(basename "${ARTIFACTS[0]}")"
