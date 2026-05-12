# 02 – OpenMP: Boids / Flocking

Ez a projekt a 01-es boids feladat OpenMP-s valtozata. A vilag toroid modon viselkedik, tehat a kepernyo egyik oldalan kilepo objektum a masik oldalon jelenik meg ujra. A cel az, hogy ugyanaz a jatek- es app-reteg fusson, mint a 01-esben, csak a parhuzamos frissites OpenMP-vel tortenjen.

A lenyegi OpenMP resz a `src/boids.c` fajlban van: a boid tomb szeletekre bomlik, es a vilagfrissites OpenMP szalak kozott fut le.

## Forditas

```powershell
.\make.cmd
```

Ez felepiti a normal futtatot es a kulon benchmark futtatot is.

## Futtatas

Normal inditas:

```powershell
.\boids_openmp.exe
```

Indulaskor feljon egy kis ablak, ahol megadhato a boidok szama es a szalak szama. Ha a szalak szama 1, akkor soros futas indul, ha nagyobb, akkor OpenMP-s futas indul.

Parancssori inditas is hasznalhato:

```powershell
.\boids_openmp.exe --mode openmp --threads 4 --boids 200
```

## Benchmark

Kulon benchmark inditas:

```powershell
.\boids_openmp_benchmark.exe
```

A benchmark futas menete:

- indulas elott lefut egy soros vs OpenMP osszehasonlitas ugyanarra a kezdo boids allapotra
- utana elindul maga a jatek eloben
- a program futas kozben a konzolba idonkent irja a meresi adatokat
- bezaraskor a meresi adatok egy `boids_openmp_benchmark_*.txt` fajlba is bekerulnek

Parancssori benchmark pelda:

```powershell
.\boids_openmp_benchmark.exe --benchmark 500 --compare --mode openmp --threads 4 --boids 200
```

## Fontos fajlok

- `src/main.c`: SDL ablakkezeles, jatekmodok, HUD, benchmark es az OpenMP-s 01-port app-retege
- `src/boids.c`: a flocking szabalyok, a soros frissites es az OpenMP-s vilagfrissites
- `src/boids.h`: kozos tipusok es fuggvenydeklaraciok

## Megjegyzes

Ha a UI assetek nem elerhetok, a program akkor is fut: ilyenkor fallback rajzolas jelenik meg a HUD elemek helyen.
