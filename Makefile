UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
PRESET ?= macos
else
PRESET ?= linux
endif

.PHONY: all docker-linux docker-windows docker-macos docker-macos-arm64 docker-switch clean

all:
	cmake --preset $(PRESET)
	cmake --build --preset $(PRESET)

docker-linux:
	bash build.sh linux

docker-windows:
	bash build.sh windows

docker-macos:
	bash build.sh macos

docker-macos-arm64:
	bash build.sh macos arm64

docker-switch:
	bash build.sh switch

clean:
	rm -rf build-* dist ROMBundler-* *.o rombundler rombundler.exe
