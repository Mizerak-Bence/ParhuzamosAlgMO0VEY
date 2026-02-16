# 03 – Windows threads (WinAPI): Boids/Flocking

## Cél
A boids szimuláció párhuzamosítása WinAPI thread-ekkel (`CreateThread`, `WaitForMultipleObjects` stb.).

## Input
- WASD: player mozgatása
- Q: kilépés

## Build/Run (MinGW)
- `make`
- `make run`

## Copy-paste (PowerShell) – gcc build + run
Fordítás:
```powershell
cd "d:\Egyetem\2025_26_2\Párhuzamos algoritmusok\Beadando\03_winapi_threads_boids"
gcc -O2 -std=c11 -Wall -Wextra -Wpedantic -DWIN32_LEAN_AND_MEAN src\*.c -o boids_winapi.exe
```

Futtatás:
```powershell
./boids_winapi.exe --threads 4 --boids 200 --seconds 10
```

Futtatás (indefinitely, amíg ki nem lépsz):
```powershell
./boids_winapi.exe --threads 4 --boids 200
```
Kilépés: `Q`

Indítás külön konzolablakban:
```powershell
Start-Process cmd -ArgumentList '/k', 'cd /d "d:\Egyetem\2025_26_2\Párhuzamos algoritmusok\Beadando\03_winapi_threads_boids" && boids_winapi.exe --threads 4 --boids 200'
```

## Megjegyzés
A cél, hogy a 01-es feladathoz hasonló logika legyen, csak a párhuzamosítási réteg legyen WinAPI.
