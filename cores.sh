#!/usr/bin/env bash
# Build a static libretro core inside a port Docker image.
# Also sourceable: provides core_resolve / core_list_keys (expects ROOT=repo root).
#
#   bash cores.sh switch genesis_plus_gx
#   bash cores.sh vita mgba
#   bash cores.sh wasm genesis
#
# Core names are defined in cores.env (NAME=git-url). Lookup is case-insensitive.
#
# Output: cores/<port>/*_libretro*.a (emscripten may emit *.bc that is actually ar → renamed .a)
# Optional: MAKE_FLAGS="HAVE_CHD=0" bash cores.sh switch genesis_plus_gx
# Optional: SKIP_DOCKER_BUILD=1 (reuse existing rombundler-<port> image)

core_normalize_key() {
	printf '%s' "$1" | tr '[:lower:]' '[:upper:]' | tr '-' '_'
}

core_list_keys() {
	local envf="${ROOT}/cores.env"
	local line key
	while IFS= read -r line || [[ -n "${line}" ]]; do
		[[ -z "${line}" || "${line}" =~ ^[[:space:]]*# ]] && continue
		key="${line%%=*}"
		[[ -n "${key}" ]] && printf '%s\n' "${key}"
	done < "${envf}"
}

# Resolve NAME from cores.env into CORE_KEY + CORE_URL. Returns 1 if unknown.
core_resolve() {
	local raw="${1:-}"
	local envf="${ROOT}/cores.env"
	local want line key val

	CORE_KEY=""
	CORE_URL=""

	if [[ -z "${raw}" ]]; then
		return 1
	fi

	if [[ ! -f "${envf}" ]]; then
		echo "missing ${envf}" >&2
		return 1
	fi

	want="$(core_normalize_key "${raw}")"
	while IFS= read -r line || [[ -n "${line}" ]]; do
		[[ -z "${line}" || "${line}" =~ ^[[:space:]]*# ]] && continue
		key="${line%%=*}"
		val="${line#*=}"
		if [[ "$(core_normalize_key "${key}")" == "${want}" ]]; then
			CORE_KEY="${key}"
			CORE_URL="${val}"
			return 0
		fi
	done < "${envf}"
	return 1
}

# --- CLI (skipped when sourced from build.sh / CI) ---
if [[ "${BASH_SOURCE[0]:-}" != "${0:-}" ]]; then
	return 0
fi

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "${ROOT}"

USAGE="usage: $0 switch|vita|wasm <core_name>
examples:
  $0 switch genesis_plus_gx
  $0 vita genesis
  MAKE_FLAGS='HAVE_CHD=0' $0 switch genesis
see cores.env for the full name list (case-insensitive)"

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

if ! core_resolve "${RAW}"; then
	echo "unknown core '${RAW}' — add it to cores.env or pick a name from:" >&2
	core_list_keys | sed 's/^/  /' >&2
	exit 1
fi

CORE_SLUG="${CORE_KEY}"
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
echo "core: ${CORE_KEY}"
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
# Emscripten: plain `make` uses host cc and produces ELF .o files that wasm-ld
# skips ("neither Wasm object file nor LLVM bitcode") → undefined retro_* symbols.
# emmake injects CC=emcc CXX=em++ AR=emar.
# shellcheck disable=SC2086
if [[ "${PORT}" == "wasm" ]]; then
	# Side modules (build.sh --side-module) reject non-PIC objects
	# ("recompile with -fPIC"). EMCC_CFLAGS covers compiles that ignore $(fpic).
	docker run --rm \
		"${DOCKER_PLATFORM[@]}" \
		-u "$(id -u):$(id -g)" \
		-v "${ROOT}:/src" \
		-w "/src/${BUILD_DIR}" \
		-e HOME=/tmp \
		-e EMCC_CFLAGS="${EMCC_CFLAGS:-} -fPIC" \
		"${IMAGE}" \
		bash -lc "emmake make -f Makefile.libretro platform=${MAKE_PLATFORM} fpic=-fPIC -j\"\$(nproc)\" ${MAKE_FLAGS}"
else
	docker run --rm \
		"${DOCKER_PLATFORM[@]}" \
		-u "$(id -u):$(id -g)" \
		-v "${ROOT}:/src" \
		-w "/src/${BUILD_DIR}" \
		-e HOME=/tmp \
		"${IMAGE}" \
		bash -lc "make -f Makefile.libretro platform=${MAKE_PLATFORM} -j\"\$(nproc)\" ${MAKE_FLAGS}"
fi

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
	base="$(basename "${art}")"
	dest="${OUT_DIR}/${base}"
	# STATIC_LINKING cores (e.g. Genesis on emscripten) write an ar archive
	# but name it *.bc. emcc treats *.bc as LLVM bitcode and fails with
	# "expected integer" / "!<arch>". Normalize to *.a for linking.
	if [[ "$(head -c 7 "${art}" 2>/dev/null || true)" == '!<arch>' ]]; then
		dest="${OUT_DIR}/$(basename "${base}" .bc)"
		case "${dest}" in
			*.a) ;;
			*) dest="${dest}.a" ;;
		esac
	fi
	cp -f "${art}" "${dest}"
	echo "built: ${dest}"
	LAST_OUT="${dest}"
done

rm -rf "${BUILD_DIR}"
echo "done. link with: bash build.sh ${PORT} ${LAST_OUT}"
