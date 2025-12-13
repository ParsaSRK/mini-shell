CC := gcc

CFLAGS := -std=c11 -Wall -Wextra -Wpedantic
DEBUG_FLAGS := -g -O0 -fsanitize=address -fno-omit-frame-pointer
RELEASE_FLAGS := -O2 -DNDEBUG
LDFLAGS :=

CONFIG ?= debug

ifeq ($(CONFIG),debug)
	CFLAGS += $(DEBUG_FLAGS)
	LDFLAGS += -fsanitize=address
else
	CFLAGS += $(RELEASE_FLAGS)
endif


BUILDDIR := build
SRC   := $(wildcard src/*.c)
OBJ   := $(SRC:src/%.c=$(BUILDDIR)/%.o)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

mini-shell: $(OBJ)
	$(CC) $(OBJ) -o $(BUILDDIR)/$@ $(LDFLAGS)

.PHONY: all clean

all: mini-shell

clean:
	rm -rf $(BUILDDIR) mini-shell

