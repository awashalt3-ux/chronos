CC      = gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=

ifeq ($(OS),Windows_NT)
LDLIBS  = -lshell32 -luser32
endif

SRC    := $(wildcard src/*.c)
OBJ    := $(SRC:.c=.o)
TARGET := chronos

ifeq ($(OS),Windows_NT)
TARGET := chronos.exe
endif

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
ifeq ($(OS),Windows_NT)
	-del /Q $(subst /,\,$(OBJ)) $(TARGET)
else
	-rm -f $(OBJ) $(TARGET)
endif

.PHONY: all clean