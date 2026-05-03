CC=gcc
CFLAGS=-O2 -std=c11 -Wall -Wextra -Wpedantic

PKG_CONFIG?=pkg-config
SDL2_CFLAGS?=$(shell $(PKG_CONFIG) --cflags sdl2)
SDL2_LDFLAGS?=$(shell $(PKG_CONFIG) --libs sdl2)
SDL2_LDFLAGS_CONSOLE:=$(filter-out -mwindows,$(SDL2_LDFLAGS))

CPPFLAGS+=$(SDL2_CFLAGS)

SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)
BIN=boids_openmp.exe
BENCH_BIN=boids_openmp_benchmark.exe

all: $(BIN) $(BENCH_BIN)

benchmark: $(BENCH_BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(SDL2_LDFLAGS) -fopenmp

$(BENCH_BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(SDL2_LDFLAGS_CONSOLE) -fopenmp

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

run: $(BIN)
	./$(BIN) --threads 4 --boids 200 --width 80 --height 25

clean:
	-del /Q src\*.o $(BIN) $(BENCH_BIN) life_openmp.exe life_openmp_benchmark.exe 2>nul
