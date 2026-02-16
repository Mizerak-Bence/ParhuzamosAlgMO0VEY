CC=gcc
CFLAGS=-O2 -std=c11 -Wall -Wextra -Wpedantic
LDFLAGS=

SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)
BIN=boids_pthreads.exe

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -pthread

run: $(BIN)
	./$(BIN) --mode pthread --threads 4 --boids 200

clean:
	-del /Q src\*.o $(BIN) 2>nul
