# 03 – Windows threads (WinAPI): Boids/Flocking

## Cél
A boids szimuláció párhuzamosítása WinAPI thread-ekkel (`CreateThread`, `WaitForMultipleObjects` stb.).

Az aktuális változat SDL2 ablakban jeleníti meg a boidokat, tehát már nem konzolban frissül a kép.

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
.\make.cmd
```

Futtatás:
```powershell
./boids_winapi.exe --threads 4 --boids 200
```
Kilépés: `Q` vagy az ablak bezárása.

## Megjegyzés
A cél, hogy a 01-es feladathoz hasonló logika legyen, csak a párhuzamosítási réteg legyen WinAPI.
