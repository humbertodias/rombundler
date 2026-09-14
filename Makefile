UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
PRESET ?= macos
else
PRESET ?= linux
endif

.PHONY: all clean

all:
	cmake --preset $(PRESET)
	cmake --build --preset $(PRESET)

clean:
	rm -rf build-* dist ROMBundler-* *.o rombundler rombundler.exe
