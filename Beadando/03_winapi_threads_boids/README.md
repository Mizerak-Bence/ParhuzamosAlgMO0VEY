# 03 – Windows threads (WinAPI): Boids/Flocking

## Cél
A boids szimuláció párhuzamosítása WinAPI thread-ekkel (`CreateThread`, `WaitForMultipleObjects` stb.).

Az aktuális változat SDL2 ablakban jeleníti meg a boidokat, 01-szerűbb csoportos flockinggal és külön benchmark futtatható változattal.

## Input
- WASD: player mozgatása
- Q: kilépés

## Build/Run (MinGW)
- `make`
- `make benchmark`
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

Induláskor feljön egy kis ablak, ahol megadható a boid darabszám és a szálak száma.
Benchmark indításnál ugyanitt az összevetési lépések száma is megadható.

## Benchmark

Kulon benchmark futtathato:

```powershell
.\make.cmd benchmark
.\boids_winapi_benchmark.exe
```

A benchmark futás menete:

- indulás előtt lefut egy soros vs WinAPI összehasonlítás
- utána elindul maga a boids szimuláció élőben
- futás közben a konzolba kb. másodpercenként élő interval sorok íródnak
- az ablak címsorában is látszik a seq átlag, a WinAPI átlag, a speedup és az élő ms/tick érték
- bezáráskor készül egy `boids_winapi_benchmark_*.txt` naplófájl is

Parancssori benchmark továbbra is használható:

```powershell
.\boids_winapi_benchmark.exe --benchmark 500 --compare --threads 4 --boids 200 --width 80 --height 25
```

Parancssori élő benchmark session is indítható prompt nélkül:

```powershell
.\boids_winapi_benchmark.exe --live-benchmark 30 --runtime 3 --threads 4 --boids 200 --width 80 --height 25
```

Itt a `--live-benchmark` az előzetes összehasonlításhoz használt tickszámot adja meg, a `--runtime` pedig másodpercben megadja, mennyi idő után lépjen ki automatikusan az élő ablak.

## Megjegyzés
A cél, hogy a 01-es feladathoz hasonló logika legyen, csak a párhuzamosítási réteg legyen WinAPI.
