# 01 – POSIX pthreads: Boids/Flocking

## Cél
Boids/flocking szimuláció párhuzamosítása POSIX `pthreads` használatával, és összevetés soros futással.

## Input
- WASD: player mozgatása (a megnyitott ablak legyen fókuszban)
- Q vagy ESC: kilépés
- Ablak bezárása (X): kilépés

## (PowerShell) – gcc build 
Fordítás:
```powershell
gcc -O2 -std=c11 -Wall -Wextra -Wpedantic src\*.c -o boids_pthreads.exe -pthread \
	-IC:\\path\\to\\SDL2\\include -LC:\\path\\to\\SDL2\\lib -lSDL2main -lSDL2 -mwindows
```

Futtatás (pthreads):
```powershell
./boids_pthreads.exe --mode pthread --threads 4 --boids 200
```

Megjegyzés: ha a `-pthread` nem működne a toolchain-ben, próbáld ezt:
```powershell
gcc -O2 -std=c11 -Wall -Wextra -Wpedantic src\*.c -o boids_pthreads.exe -lwinpthread \
	-IC:\\path\\to\\SDL2\\include -LC:\\path\\to\\SDL2\\lib -lSDL2main -lSDL2 -mwindows
```

Megjegyzés: Makefile-lal is átadhatod az SDL2 útvonalakat:
```powershell
mingw32-make SDL2_CFLAGS="-IC:\\path\\to\\SDL2\\include" SDL2_LDFLAGS="-LC:\\path\\to\\SDL2\\lib -lSDL2main -lSDL2 -mwindows"
```
