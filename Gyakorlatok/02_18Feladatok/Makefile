CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -O2

# Usage examples:
#   mingw32-make FILE=Meres
#   mingw32-make run FILE=Meres ARGS="meres.csv 1000000"
#   mingw32-make clean

FILE ?= Hellow
EXE := $(FILE).exe

.PHONY: all run clean help

all: $(EXE)

$(EXE): $(FILE).c
	$(CC) $(CFLAGS) $< -o $@

run: $(EXE)
	./$(EXE) $(ARGS)

clean:
	del /q *.o *.exe 2>nul || exit 0

help:
	@echo Targets:
	@echo   mingw32-make FILE=Meres
	@echo   mingw32-make run FILE=Meres ARGS="meres.csv 1000000"
	@echo   mingw32-make clean
