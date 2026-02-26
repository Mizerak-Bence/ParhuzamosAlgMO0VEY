CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -O2

# Usage examples:
#   mingw32-make FILE=F1_CrewPrefix_HivasiFa
#   mingw32-make run FILE=F1_CrewPrefix_HivasiFa ARGS="16 crew_prefix.dot"

FILE ?= F1_CrewPrefix_HivasiFa
EXE := $(FILE).exe

.PHONY: all run clean help

all: $(EXE)

$(EXE): $(FILE).c
	$(CC) $(CFLAGS) $< -o $@

run: $(EXE)
	./$(EXE) $(ARGS)

clean:
	del /q *.o *.exe *.dot 2>nul || exit 0

help:
	@echo Targets:
	@echo   mingw32-make FILE=F1_CrewPrefix_HivasiFa
	@echo   mingw32-make run FILE=F1_CrewPrefix_HivasiFa ARGS="16 crew_prefix.dot"
	@echo   mingw32-make clean
