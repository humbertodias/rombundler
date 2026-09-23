#ifndef ROMBUNDLER_WASM_CORE_LOAD_H
#define ROMBUNDLER_WASM_CORE_LOAD_H

#if defined(ROMBUNDLER_WASM_DYNAMIC)
#include <dlfcn.h>
#define load_sym(V, S) do { \
	if (!((*(void**)&V) = dlsym(core.handle, #S))) \
		die("Failed to load symbol '" #S "': %s", dlerror()); \
	} while (0)
#define load_lib(L) dlopen((L), RTLD_NOW | RTLD_GLOBAL)
#define close_lib(L) dlclose(L)
#define load_retro_sym(S) load_sym(core.S, S)
#else
#include "../static/core_load.h"
#endif

#endif
