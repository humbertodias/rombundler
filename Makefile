UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
PRESET ?= macos
else
PRESET ?= linux
endif

.PHONY: all docker-linux docker-windows docker-macos docker-macos-arm64 clean

all:
	cmake --preset $(PRESET)
	cmake --build --preset $(PRESET)

docker-linux:
	bash scripts/build.sh linux

docker-windows:
	bash scripts/build.sh windows

docker-macos:
	bash scripts/build.sh macos

docker-macos-arm64:
	bash scripts/build.sh macos arm64

clean:
	rm -rf build-* dist ROMBundler-* *.o rombundler rombundler.exe
