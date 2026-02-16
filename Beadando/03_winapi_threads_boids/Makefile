CC=gcc
CFLAGS=-O2 -std=c11 -Wall -Wextra -Wpedantic -DWIN32_LEAN_AND_MEAN

SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)
BIN=boids_winapi.exe

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

run: $(BIN)
	./$(BIN) --threads 4 --boids 200

clean:
	-del /Q src\*.o $(BIN) 2>nul
