CC := gcc

CFLAGS := -std=c11 -Wall -Wextra -Wpedantic \
		  -D_POSIX_C_SOURCE=200809L \
		  -I./include
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
TARGET := $(BUILDDIR)/mini-shell
SRC   := $(wildcard src/*.c)
OBJ   := $(SRC:src/%.c=$(BUILDDIR)/%.o)

all: $(TARGET)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

.PHONY: all clean docs clean-docs

clean:
	rm -rf $(BUILDDIR)

docs:
	doxygen Doxyfile

clean-docs:
	rm -rf docs
