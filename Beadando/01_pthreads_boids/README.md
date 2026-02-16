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
gcc -O2 -std=c11 -Wall -Wextra -Wpedantic src\*.c -o boids_pthreads.exe -pthread -lgdi32 -luser32
```

Futtatás (pthreads):
```powershell
./boids_pthreads.exe --mode pthread --threads 4 --boids 200
```

Megjegyzés: ha a `-pthread` nem működne a toolchain-ben, próbáld ezt:
```powershell
gcc -O2 -std=c11 -Wall -Wextra -Wpedantic src\*.c -o boids_pthreads.exe -lwinpthread -lgdi32 -luser32
```
