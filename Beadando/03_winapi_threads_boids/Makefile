CC=gcc
CFLAGS=-O2 -std=c11 -Wall -Wextra -Wpedantic -DWIN32_LEAN_AND_MEAN

PKG_CONFIG?=pkg-config
SDL2_CFLAGS?=$(shell $(PKG_CONFIG) --cflags sdl2)
SDL2_LDFLAGS?=$(shell $(PKG_CONFIG) --libs sdl2)
SDL2_LDFLAGS_CONSOLE:=$(filter-out -mwindows,$(SDL2_LDFLAGS))

CPPFLAGS+=$(SDL2_CFLAGS)

SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)
BIN=boids_winapi.exe
BENCH_BIN=boids_winapi_benchmark.exe

all: $(BIN) $(BENCH_BIN)

benchmark: $(BENCH_BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(SDL2_LDFLAGS)

$(BENCH_BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(SDL2_LDFLAGS_CONSOLE)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

run: $(BIN)
	./$(BIN) --threads 4 --boids 200

clean:
	-del /Q src\*.o $(BIN) $(BENCH_BIN) 2>nul
