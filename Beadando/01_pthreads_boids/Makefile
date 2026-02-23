CC=gcc
CFLAGS=-O2 -std=c11 -Wall -Wextra -Wpedantic

# SDL2 settings (set these from the command line if needed)
# Example:
#   mingw32-make SDL2_CFLAGS="-IC:/SDL2/include" SDL2_LDFLAGS="-LC:/SDL2/lib -lSDL2main -lSDL2 -mwindows"
PKG_CONFIG?=pkg-config
SDL2_CFLAGS?=$(shell where $(PKG_CONFIG) >nul 2>nul && $(PKG_CONFIG) --cflags sdl2 2>nul)
SDL2_LDFLAGS?=$(shell where $(PKG_CONFIG) >nul 2>nul && $(PKG_CONFIG) --libs sdl2 2>nul)

SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)
BIN=boids_pthreads.exe

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(SDL2_CFLAGS) -o $@ $^ $(SDL2_LDFLAGS) -pthread

run: $(BIN)
	./$(BIN) --mode pthread --threads 4 --boids 200

clean:
	-del /Q src\*.o $(BIN) 2>nul
