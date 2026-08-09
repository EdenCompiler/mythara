CC ?= cc
BUILD_DIR ?= build/make
AUDIO ?= 1
RELEASE ?= 0

FONTE := src/mythara.c
EXECUTAVEL := $(BUILD_DIR)/mythara
CPPFLAGS :=
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic
LDLIBS :=

ifeq ($(RELEASE),1)
    CFLAGS += -O2 -DNDEBUG
else
    CFLAGS += -O0 -g
endif

ifeq ($(OS),Windows_NT)
    EXECUTAVEL := $(BUILD_DIR)/mythara.exe
    LDLIBS += -lgdi32 -luser32 -lshell32 -lm
    ifeq ($(AUDIO),1)
        LDLIBS += -lwinmm
    else
        CPPFLAGS += -DMYTHARA_SEM_AUDIO
    endif
else
    LDLIBS += -lX11 -lm -pthread
    ifeq ($(AUDIO),1)
        LDLIBS += -lasound
    else
        CPPFLAGS += -DMYTHARA_SEM_AUDIO
    endif
endif

.PHONY: all test run release sem-audio package clean

all: $(EXECUTAVEL)

$(EXECUTAVEL): $(FONTE) Makefile
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(FONTE) -o $(EXECUTAVEL) $(LDLIBS)

test: $(EXECUTAVEL)
	$(EXECUTAVEL) --autoteste

run: $(EXECUTAVEL)
	$(EXECUTAVEL)

release:
	$(MAKE) RELEASE=1 all

sem-audio:
	$(MAKE) AUDIO=0 all

package:
	./scripts/empacotar.sh

clean:
	$(RM) -r build dist
