TARGET  := rombundler
APP     := ROMBundler
VERSION ?= devel
ARCH    ?= $(shell uname -m)
APP.DIR := $(APP)-$(ARCH).app
DEPS    ?=

# Windows cmd.exe exports OS=Windows_NT. Docker/cross builds pass OS=Linux|Windows|OSX.
ifeq ($(OS),Windows_NT)
override OS := Windows
endif

ifeq ($(OS),)
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),)
		OS := Windows
	else ifneq ($(findstring MINGW,$(UNAME_S)),)
		OS := Windows
	else ifneq ($(findstring Darwin,$(UNAME_S)),)
		OS := OSX
	else
		OS := Linux
	endif
endif

ifeq ($(OS),Windows)
	TARGET := rombundler.exe
	CFLAGS  += -DAL_LIBTYPE_STATIC
	ifneq ($(DEPS),)
		CFLAGS  += -I$(DEPS)/include
		LDFLAGS += -L$(DEPS)/lib -lglfw3 -lopenal \
			-lgdi32 -luser32 -lshell32 -limm32 -lole32 -luuid \
			-lwinmm -lksuser -lavrt -lsetupapi -lcfgmgr32 \
			-lopengl32 -static-libgcc -static-libstdc++ -lstdc++ -mwindows
	else
		LDFLAGS += -L./lib -lglfw3 -lopenal -lgdi32 -luser32 -lopengl32 -mwindows
	endif
else ifeq ($(OS),OSX)
	ifneq ($(DEPS),)
		CFLAGS  += -I$(DEPS)/include
		LDFLAGS := -L$(DEPS)/lib -lglfw3 -lopenal
	else
		LDFLAGS := $(shell pkg-config --libs glfw3 openal 2>/dev/null || echo -lglfw3 -lopenal)
	endif
	LDFLAGS += -framework Cocoa -framework IOKit -framework CoreFoundation \
		-framework CoreVideo -framework OpenGL \
		-framework AudioToolbox -framework CoreAudio -framework AudioUnit -lc++
else
	ifneq ($(DEPS),)
		CFLAGS  += -I$(DEPS)/include
		LDFLAGS := -L$(DEPS)/lib
		LDFLAGS += $(shell PKG_CONFIG_PATH=$(DEPS)/lib/pkgconfig pkg-config --static --libs glfw3 openal)
		LDFLAGS += -lstdc++ -lm
	else
		LDFLAGS := -lm
		LDFLAGS += $(shell pkg-config --libs glfw3 2>/dev/null || echo -lglfw3)
		LDFLAGS += $(shell pkg-config --libs openal 2>/dev/null || echo -lopenal)
	endif
endif

# osxcross ld cannot link LLVM bitcode produced by -flto.
ifneq ($(OS),OSX)
	LTOFLAGS := -flto
endif

CFLAGS += -Wall -O3 -fPIC $(LTOFLAGS) -I. -Iinclude -Ideps/include

OBJ = main.o glad.o config.o core.o audio.o video.o input.o options.o ini.o utils.o srm.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: all clean bundle docker-linux docker-windows docker-macos docker-macos-arm64

all: $(TARGET)
$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LTOFLAGS)

bundle: $(TARGET)
	rm -rf ROMBundler-$(OS)-$(VERSION)-$(ARCH)
	mkdir -p ROMBundler-$(OS)-$(VERSION)-$(ARCH)
	cp $(TARGET) ROMBundler-$(OS)-$(VERSION)-$(ARCH)
	cp config.ini ROMBundler-$(OS)-$(VERSION)-$(ARCH)
	cp README.md ROMBundler-$(OS)-$(VERSION)-$(ARCH)
	cp COPYING ROMBundler-$(OS)-$(VERSION)-$(ARCH)
	zip -r ROMBundler-$(OS)-$(VERSION)-$(ARCH).zip ROMBundler-$(OS)-$(VERSION)-$(ARCH)

$(APP).app: $(TARGET)
	mkdir -p $(APP.DIR)/Contents/MacOS
	mkdir -p $(APP.DIR)/Contents/Resources/$(APP).iconset
	cp bundle/Darwin/Info.plist $(APP.DIR)/Contents/
	sed -i.bak 's/0.1.0/$(VERSION)/' $(APP.DIR)/Contents/Info.plist
	rm $(APP.DIR)/Contents/Info.plist.bak
	echo "APPL????" > $(APP.DIR)/Contents/PkgInfo
	cp config.ini $(APP.DIR)/
	sips -z 16 16   bundle/Darwin/icon.png --out $(APP.DIR)/Contents/Resources/$(APP).iconset/icon_16x16.png
	sips -z 32 32   bundle/Darwin/icon.png --out $(APP.DIR)/Contents/Resources/$(APP).iconset/icon_16x16@2x.png
	sips -z 32 32   bundle/Darwin/icon.png --out $(APP.DIR)/Contents/Resources/$(APP).iconset/icon_32x32.png
	sips -z 64 64   bundle/Darwin/icon.png --out $(APP.DIR)/Contents/Resources/$(APP).iconset/icon_32x32@2x.png
	sips -z 128 128 bundle/Darwin/icon.png --out $(APP.DIR)/Contents/Resources/$(APP).iconset/icon_128x128.png
	sips -z 256 256 bundle/Darwin/icon.png --out $(APP.DIR)/Contents/Resources/$(APP).iconset/icon_128x128@2x.png
	sips -z 256 256 bundle/Darwin/icon.png --out $(APP.DIR)/Contents/Resources/$(APP).iconset/icon_256x256.png
	sips -z 512 512 bundle/Darwin/icon.png --out $(APP.DIR)/Contents/Resources/$(APP).iconset/icon_256x256@2x.png
	sips -z 512 512 bundle/Darwin/icon.png --out $(APP.DIR)/Contents/Resources/$(APP).iconset/icon_512x512.png
	cp bundle/Darwin/launcher.command $(APP.DIR)/Contents/MacOS
	cp rombundler $(APP.DIR)/Contents/MacOS
	iconutil -c icns -o $(APP.DIR)/Contents/Resources/$(APP).icns $(APP.DIR)/Contents/Resources/$(APP).iconset
	rm -rf $(APP.DIR)/Contents/Resources/$(APP).iconset
	zip -r ROMBundler-$(OS)-$(VERSION)-$(ARCH).app.zip $(APP.DIR)

docker-linux:
	bash scripts/build.sh linux

docker-windows:
	bash scripts/build.sh windows

docker-macos:
	bash scripts/build.sh macos

docker-macos-arm64:
	bash scripts/build.sh macos arm64

clean:
	rm -rf $(OBJ) $(TARGET) ROMBundler-* ROMBundler-$(ARCH).*
