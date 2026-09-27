#!/usr/bin/env bash
# Build a static libretro core inside a port Docker image.
# Also sourceable: provides core_resolve / core_list_keys (expects ROOT=repo root).
#
#   bash cores.sh switch genesis_plus_gx
#   bash cores.sh vita mgba
#   bash cores.sh wasm genesis
#
# Core names are defined in cores.env (NAME=git-url,branch,command).
# Lookup is case-insensitive.
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

# Resolve NAME from cores.env into CORE_KEY, CORE_URL, CORE_BRANCH, CORE_CMD.
# Line format: NAME=git-url,branch,command
# The command may contain commas; only the first two commas separate fields.
# Returns 1 if unknown.
core_resolve() {
	local raw="${1:-}"
	local envf="${ROOT}/cores.env"
	local want line key rest url branch cmd

	CORE_KEY=""
	CORE_URL=""
	CORE_BRANCH=""
	CORE_CMD=""

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
		rest="${line#*=}"
		if [[ "$(core_normalize_key "${key}")" == "${want}" ]]; then
			url="${rest%%,*}"
			rest="${rest#*,}"
			branch="${rest%%,*}"
			cmd="${rest#*,}"
			url="${url#"${url%%[![:space:]]*}"}"
			url="${url%"${url##*[![:space:]]}"}"
			branch="${branch#"${branch%%[![:space:]]*}"}"
			branch="${branch%"${branch##*[![:space:]]}"}"
			cmd="${cmd#"${cmd%%[![:space:]]*}"}"
			cmd="${cmd%"${cmd##*[![:space:]]}"}"
			CORE_KEY="${key}"
			CORE_URL="${url}"
			CORE_BRANCH="${branch}"
			CORE_CMD="${cmd}"
			return 0
		fi
	done < "${envf}"
	return 1
}

# Build the docker command from CORE_CMD. Make gets platform=, -j, and MAKE_FLAGS.
# Wasm prefixes emmake / emcmake. $(nproc) is left for the container shell.
core_assemble_cmd() {
	local cmd="${CORE_CMD:-make -f Makefile.libretro}"

	if [[ "${cmd}" == make\ * ]]; then
		if [[ "${cmd}" != *" platform="* ]]; then
			cmd+=" platform=${MAKE_PLATFORM}"
		fi
		if [[ "${PORT}" == "wasm" && "${cmd}" != *" fpic="* ]]; then
			cmd+=" fpic=-fPIC"
		fi
		if [[ "${cmd}" != *" -j"* ]]; then
			cmd+=' -j$(nproc)'
		fi
		if [[ -n "${MAKE_FLAGS:-}" ]]; then
			cmd+=" ${MAKE_FLAGS}"
		fi
		if [[ "${PORT}" == "wasm" ]]; then
			cmd="emmake ${cmd}"
		fi
	elif [[ "${cmd}" == cmake\ * ]]; then
		if [[ "${cmd}" != *" -j"* ]]; then
			cmd+=' -j$(nproc)'
		fi
		if [[ "${PORT}" == "wasm" ]]; then
			cmd="emcmake ${cmd}"
		fi
	elif [[ "${cmd}" == cargo\ * && "${PORT}" == "wasm" ]]; then
		if [[ "${cmd}" != *" --target "* && "${cmd}" != *"--target="* ]]; then
			cmd+=" --target wasm32-unknown-emscripten"
		fi
	elif [[ "${cmd}" == cargo\ * && "${PORT}" == "switch" ]]; then
		# Host cargo emits x86_64 ELF (EM: 62); aarch64-none-elf-ld cannot link it.
		if [[ "${cmd}" != *" --target "* && "${cmd}" != *"--target="* ]]; then
			cmd+=" --target aarch64-unknown-linux-gnu"
		fi
	fi
	printf '%s\n' "${cmd}"
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

# The Zeebx staticlib is built from the zeebx tree. Source changes and the
# aarch64/Horizon flags live there, not in a clone patched at this step.
if [[ "${PORT}" == "switch" && "${CORE_KEY}" == "ZEEBX" ]]; then
	ZEEBX_ROOT="${ZEEBX_ROOT:-$(cd "${ROOT}/../zeebx-emu" && pwd)}"
	if [[ ! -x "${ZEEBX_ROOT}/frontends/switch/compilar.sh" ]]; then
		echo "Zeebx Switch build not found: ${ZEEBX_ROOT}/frontends/switch/compilar.sh" >&2
		echo "Set ZEEBX_ROOT to the zeebx-emu checkout." >&2
		exit 1
	fi
	mkdir -p "${OUT_DIR}"
	ZEEBX_SWITCH_IMAGE="${IMAGE}" bash "${ZEEBX_ROOT}/frontends/switch/compilar.sh" "${ROOT}/${OUT_DIR}"
	echo "done. link with: bash build.sh switch ${OUT_DIR}/libzeebx_libretro.a"
	exit 0
fi

MAKE_FLAGS="${MAKE_FLAGS:-}"
RUN_CMD="$(core_assemble_cmd)"

echo "image: ${IMAGE}"
echo "port: ${PORT} (make platform=${MAKE_PLATFORM})"
echo "core: ${CORE_KEY}"
echo "url: ${CORE_URL}"
echo "branch: ${CORE_BRANCH}"
echo "cmd: ${RUN_CMD}"
echo "out: ${OUT_DIR}/"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}" "${OUT_DIR}"

# Clone on the host (auth/network), build inside the toolchain image.
clone_args=(--depth 1)
if [[ -n "${CORE_BRANCH}" ]]; then
	clone_args+=(--branch "${CORE_BRANCH}")
fi
git clone "${clone_args[@]}" "${CORE_URL}" "${BUILD_DIR}"

# Make cores name their makefile with -f / -C. Confirm it exists before docker.
if [[ "${CORE_CMD:-make -f Makefile.libretro}" == make\ * ]]; then
	mf_dir="."
	mf_file="Makefile.libretro"
	prev=""
	for w in ${CORE_CMD:-make -f Makefile.libretro}; do
		if [[ "${prev}" == "-C" ]]; then
			mf_dir="${w}"
		elif [[ "${prev}" == "-f" ]]; then
			mf_file="${w}"
		fi
		prev="${w}"
	done
	if [[ ! -f "${BUILD_DIR}/${mf_dir}/${mf_file}" ]]; then
		echo "no ${mf_dir}/${mf_file} in ${CORE_URL} (branch ${CORE_BRANCH})" >&2
		exit 1
	fi
fi

# Emscripten: plain `make` uses host cc and produces ELF .o files that wasm-ld
# skips ("neither Wasm object file nor LLVM bitcode") → undefined retro_* symbols.
# emmake injects CC=emcc CXX=em++ AR=emar. EMCC_CFLAGS=-fPIC covers compiles
# that ignore $(fpic); side modules reject non-PIC objects.
DOCKER_ENV=(-e HOME=/tmp -e CORE_BUILD_CMD="${RUN_CMD}")
if [[ "${PORT}" == "wasm" ]]; then
	DOCKER_ENV+=(-e EMCC_CFLAGS="${EMCC_CFLAGS:-} -fPIC")
	# Side modules reject non-PIC objects. Rust does not read EMCC_CFLAGS.
	if [[ "${CORE_CMD:-}" == cargo\ * ]]; then
		DOCKER_ENV+=(-e RUSTFLAGS="${RUSTFLAGS:-} -C relocation-model=pic")
	fi
fi
docker run --rm \
	"${DOCKER_PLATFORM[@]}" \
	-u "$(id -u):$(id -g)" \
	-v "${ROOT}:/src" \
	-w "/src/${BUILD_DIR}" \
	"${DOCKER_ENV[@]}" \
	"${IMAGE}" \
	bash -lc 'set -euo pipefail
eval "$CORE_BUILD_CMD"'

ARTIFACTS=()
while IFS= read -r -d '' art; do
	ARTIFACTS+=("${art}")
done < <(find "${BUILD_DIR}" -type f \( -name '*_libretro*.a' -o -name '*_libretro*.bc' \) -print0)
if [[ ${#ARTIFACTS[@]} -eq 0 ]]; then
	echo "build finished but no *_libretro*.a / *.bc found under ${BUILD_DIR}" >&2
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
