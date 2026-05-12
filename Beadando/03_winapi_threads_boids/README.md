# 03 – WinAPI Threads: Boids / Flocking

Ez a projekt a 01-es boids feladat WinAPI-s szálkezelésű változata. A cél itt is ugyanaz: a 01-es játékréteg maradjon meg, csak a párhuzamos világfrissítés WinAPI worker threadekkel történjen.

## Fordítás

```powershell
.\make.cmd
```

Ez felépíti a normál futtatót és a külön benchmark futtatót is.

## Futtatás

Normál indítás:

```powershell
.\boids_winapi.exe
```

Induláskor feljön egy kis ablak, ahol megadható a boidok száma és a szálak száma. Ha a szálak száma 1, akkor soros futás indul, ha nagyobb, akkor WinAPI-s párhuzamos futás indul.

Parancssori indítás is használható:

```powershell
.\boids_winapi.exe --mode winapi --threads 4 --boids 200 
```

## Benchmark

Külön benchmark indítás:

```powershell
.\boids_winapi_benchmark.exe
```

A benchmark futás menete:

- indulás előtt lefut egy soros vs WinAPI összehasonlítás ugyanarra a kezdő boids állapotra
- utána elindul maga a játék élőben
- a program futás közben a konzolba időnként kiírja a mérési adatokat
- bezáráskor a mérési adatok egy `boids_winapi_benchmark_*.txt` fájlba is bekerülnek

Parancssori benchmark példa:

```powershell
.\boids_winapi_benchmark.exe --benchmark 500 --compare --mode winapi --threads 4 --boids 200 
```

## Fontos fájlok

- `src/main.c`: SDL ablakkezelés, játékmódok, HUD, HP-bar, shockwave, benchmark és a WinAPI-s 01-port app-rétege
- `src/boids.c`: a flocking szabályok és a soros világfrissítés alapja
- `src/update_winapi.c`: a WinAPI worker thread réteg, amely a boid tömböt szeletekre bontva frissíti
- `src/boids.h`, `src/update_winapi.h`: közös típusok és deklarációk

## Megjegyzés

Ha a UI assetek nem elérhetők, a program akkor is fut: ilyenkor fallback rajzolás jelenik meg a HUD elemek helyén.
