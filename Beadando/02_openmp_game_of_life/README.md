# 02 – OpenMP: Conway Game of Life

Ez a feladat Conway Game of Life frissítést valósít meg OpenMP-vel. A lényegi párhuzamos rész a `src/life.c` fájlban van, ahol a rács sorai OpenMP `parallel for` ciklussal frissülnek.

## Irányítás

- `WASD`: kurzor mozgatása
- `SPACE`: cella kapcsolása
- `P`: pause / run
- `Q`: kilépés

## Fordítás

Normál futtatás:

```powershell
.\make.cmd
```

Külön benchmark futtatható készítése:

```powershell
.\make.cmd benchmark
```

## Futtatás

Normál játék indítása:

```powershell
.\life_openmp.exe
```

Induláskor feljön egy kis ablak, ahol megadható a szélesség, a magasság és a szálak száma.

Parancssori indítás továbbra is használható:

```powershell
.\life_openmp.exe --threads 4 --width 80 --height 25
```

## Mérés

A mérésekhez a `life_openmp_benchmark.exe` használata ajánlott. Ez külön futtatható, induláskor feljön egy ablak, ahol megadható a szélesség, a magasság, a szálak száma és az összevetési lépések száma.

A benchmark futás menete:

- indulás előtt lefut egy soros vs OpenMP összehasonlítás
- utána elindul maga a Game of Life élőben
- futás közben az ablak címsorában látszik a seq átlag, az OpenMP átlag, a speedup és az élő ms/step érték
- bezáráskor készül egy `life_openmp_benchmark_*.txt` naplófájl is

Egyszerű indítás:

```powershell
.\life_openmp_benchmark.exe
```

Parancssori benchmark továbbra is használható:

```powershell
.\life_openmp_benchmark.exe --benchmark 500 --compare --threads 4 --width 80 --height 25
```

Ajánlott mérési opciók:

```powershell
.\life_openmp_benchmark.exe --benchmark 500 --compare --threads 2 --width 80 --height 25
.\life_openmp_benchmark.exe --benchmark 500 --compare --threads 4 --width 80 --height 25
.\life_openmp_benchmark.exe --benchmark 500 --compare --threads 4 --width 120 --height 60
.\life_openmp_benchmark.exe --benchmark 500 --compare --threads 8 --width 120 --height 60
```

## Fontos fájlok

- `src/main.c`: indítás, prompt, SDL2 ablakos megjelenítés, benchmark vezérlés
- `src/life.c`: a Game of Life logika, a soros és az OpenMP frissítés
- `src/life.h`: közös típusok és függvénydeklarációk
