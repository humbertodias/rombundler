TARGET := rombundler
VERSION ?= devel
UNAME_S := $(shell uname -s)
ARCH := $(shell uname -m)

ifeq ($(UNAME_S),) # win
	TARGET := rombundler.exe
	LDFLAGS += -L./lib -lglfw3dll -lOpenal32.dll -mwindows
	OS ?= Windows
else ifneq ($(findstring MINGW,$(UNAME_S)),) # win
	TARGET := rombundler.exe
	LDFLAGS += -L./lib -lglfw3dll -lOpenal32.dll -mwindows
	OS ?= Windows
else ifneq ($(findstring Darwin,$(UNAME_S)),) # osx
	LDFLAGS := -Ldeps/osx_$(ARCH)/lib -lglfw3 -framework Cocoa -framework OpenGL -framework IOKit
	LDFLAGS += -framework OpenAL
	OS ?= OSX
else
	LDFLAGS := -ldl
	LDFLAGS += $(shell pkg-config --libs glfw3)
	LDFLAGS += $(shell pkg-config --libs openal)
	OS ?= Linux
endif

CFLAGS += -Wall -O3 -fPIC -flto -I. -Iinclude -Ideps/include

OBJ = main.o glad.o config.o core.o audio.o video.o input.o options.o ini.o utils.o srm.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) -flto

bundle: $(TARGET)
	mkdir -p ROMBundler-$(OS)-$(VERSION)-$(ARCH)
	cp $(TARGET) ROMBundler-$(OS)-$(VERSION)-$(ARCH)
	cp *.dll ROMBundler-$(OS)-$(VERSION)-$(ARCH) || :
	cp config.ini ROMBundler-$(OS)-$(VERSION)-$(ARCH)
	cp README.md ROMBundler-$(OS)-$(VERSION)-$(ARCH)
	cp COPYING ROMBundler-$(OS)-$(VERSION)-$(ARCH)
	zip -r ROMBundler-$(OS)-$(VERSION)-$(ARCH).zip ROMBundler-$(OS)-$(VERSION)-$(ARCH)

clean:
	rm -rf $(OBJ) $(TARGET) ROMBundler-*
