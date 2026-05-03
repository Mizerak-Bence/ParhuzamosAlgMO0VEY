# 02 – OpenMP: Boids / Flocking

Ez a feladat mar boids szimulaciot futtat OpenMP-vel. A szimulacio ugyanazt a problemat fogja meg, mint a tobbi boids feladat: a raj viselkedeset a koheszio, alignment, separation es a jatekos elkerulese adja. A lenyegi parhuzamos resz a `src/boids.c` fajlban van, ahol a boid tartomany OpenMP szalak kozott van szeletelve.

## Iranyitas

- `WASD`: a jatekos negyzet mozgatasa
- `Q` vagy `ESC`: kilepes

## Forditas

Normal futtatas:

```powershell
.\make.cmd
```

Kulon benchmark futtathato keszitese:

```powershell
.\make.cmd benchmark
```

## Futtatas

Normal inditas:

```powershell
.\boids_openmp.exe
```

Indulaskor feljon egy kis ablak, ahol megadhato a szelesseg, a magassag, a boid darabszam es a szalak szama.

Parancssori inditas tovabbra is hasznalhato:

```powershell
.\boids_openmp.exe --threads 4 --boids 200 --width 80 --height 25
```

## Meres

A meresekhez a `boids_openmp_benchmark.exe` hasznalata ajanlott. Ez kulon futtathato, indulaskor feljon egy ablak, ahol megadhato a szelesseg, a magassag, a boid darabszam, a szalak szama es az osszevetesi tickek szama.

A benchmark futas menete:

- indulas elott lefut egy soros vs OpenMP osszehasonlitas ugyanarra a kezdo boids allapotra
- utana elindul maga a boids szimulacio eloben
- futas kozben az ablak cimsoraban latszik a seq atlag, az OpenMP atlag, a speedup es az elo ms/tick ertek
- bezaraskor keszul egy `boids_openmp_benchmark_*.txt` naplofajl is

Egyszeru inditas:

```powershell
.\boids_openmp_benchmark.exe
```

Parancssori benchmark tovabbra is hasznalhato:

```powershell
.\boids_openmp_benchmark.exe --benchmark 500 --compare --threads 4 --boids 200 --width 80 --height 25
```

Ajanlott meresi opciok:

```powershell
.\boids_openmp_benchmark.exe --benchmark 500 --compare --threads 2 --boids 200 --width 80 --height 25
.\boids_openmp_benchmark.exe --benchmark 500 --compare --threads 4 --boids 200 --width 80 --height 25
.\boids_openmp_benchmark.exe --benchmark 500 --compare --threads 4 --boids 400 --width 120 --height 60
.\boids_openmp_benchmark.exe --benchmark 500 --compare --threads 8 --boids 400 --width 120 --height 60
```

## Fontos fajlok

- `src/main.c`: inditas, prompt, SDL2 ablakos megjelenites, benchmark vezerles
- `src/boids.c`: a boids logika, a soros es az OpenMP frissites
- `src/boids.h`: kozos tipusok es fuggvenydeklaraciok
