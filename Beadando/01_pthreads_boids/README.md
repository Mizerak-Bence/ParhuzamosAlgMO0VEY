# 01 – POSIX pthreads: Boids/Flocking

## Cél
Boids/flocking szimuláció párhuzamosítása POSIX `pthreads` használatával, és összevetés soros futással.

## Input
- WASD: player mozgatása
- Q: kilépés

## Build/Run (MinGW)
- `make`
- `make run`

## Copy-paste (PowerShell) – gcc build + run
Fordítás:
```powershell
cd "d:\Egyetem\2025_26_2\Párhuzamos algoritmusok\Beadando\01_pthreads_boids"
gcc -O2 -std=c11 -Wall -Wextra -Wpedantic src\*.c -o boids_pthreads.exe -pthread
```

Futtatás (pthreads):
```powershell
./boids_pthreads.exe --mode pthread --threads 4 --boids 200 --seconds 10
```

Futtatás (indefinitely, amíg ki nem lépsz):
```powershell
./boids_pthreads.exe --mode pthread --threads 4 --boids 200
```
Kilépés: `Q`

Indítás külön konzolablakban (PowerShellből, a futás nem blokkolja az aktuális terminált):
```powershell
Start-Process cmd -ArgumentList '/k', 'cd /d "d:\Egyetem\2025_26_2\Párhuzamos algoritmusok\Beadando\01_pthreads_boids" && boids_pthreads.exe --mode pthread --threads 4 --boids 200'
```

Futtatás (soros / baseline):
```powershell
./boids_pthreads.exe --mode seq --threads 1 --boids 200 --seconds 10
```

Futtatás (soros, indefinitely):
```powershell
./boids_pthreads.exe --mode seq --threads 1 --boids 200
```
Kilépés: `Q`

Megjegyzés: ha a `-pthread` nem működne a toolchain-ben, próbáld ezt:
```powershell
gcc -O2 -std=c11 -Wall -Wextra -Wpedantic src\*.c -o boids_pthreads.exe -lwinpthread
```

## Mérési ötlet
- futtatás pl. 5–10 másodpercig fix `tick` ciklussal
- log: átlag ms/tick, min/max
- paraméter: `--threads N` (1,2,4,8...) és `--mode seq|pthread`
