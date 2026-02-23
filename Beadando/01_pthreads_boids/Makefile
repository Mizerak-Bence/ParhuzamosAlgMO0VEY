CC=gcc
CFLAGS=-O2 -std=c11 -Wall -Wextra -Wpedantic

# SDL2 settings (set these from the command line if needed)
# Example:
#   mingw32-make SDL2_CFLAGS="-IC:/SDL2/include" SDL2_LDFLAGS="-LC:/SDL2/lib -lSDL2main -lSDL2 -mwindows"
PKG_CONFIG?=pkg-config
SDL2_CFLAGS?=$(shell $(PKG_CONFIG) --cflags sdl2)
SDL2_LDFLAGS?=$(shell $(PKG_CONFIG) --libs sdl2)

# Ensure SDL2 include/defines are used when compiling .c -> .o
CPPFLAGS+=$(SDL2_CFLAGS)

SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)
BIN=boids_pthreads.exe

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(SDL2_LDFLAGS) -pthread

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

run: $(BIN)
	./$(BIN) --mode pthread --threads 4 --boids 200

clean:
	-del /Q src\*.o $(BIN) 2>nul
