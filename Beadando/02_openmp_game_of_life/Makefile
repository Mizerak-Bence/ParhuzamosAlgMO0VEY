CC=gcc
CFLAGS=-O2 -std=c11 -Wall -Wextra -Wpedantic

SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)
BIN=life_openmp.exe

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -fopenmp

run: $(BIN)
	./$(BIN) --threads 4 --width 80 --height 25

clean:
	-del /Q src\*.o $(BIN) 2>nul
