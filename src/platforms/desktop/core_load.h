#ifndef ROMBUNDLER_CORE_LOAD_H
#define ROMBUNDLER_CORE_LOAD_H

#if defined(_WIN32)
#include <windows.h>
#define load_lib(L) LoadLibrary(L);
#define load_sym(V, S) ((*(void**)&V) = GetProcAddress(core.handle, #S))
#define close_lib(L) ((void)(L))
#else
#include <dlfcn.h>
#define load_sym(V, S) do {\
	if (!((*(void**)&V) = dlsym(core.handle, #S))) \
		die("Failed to load symbol '" #S "': %s", dlerror()); \
	} while (0)
#define load_lib(L) dlopen(L, RTLD_LAZY);
#define close_lib(L) dlclose(L);
#endif

#define load_retro_sym(S) load_sym(core.S, S)

#endif
