# Pthreads boids beadandó

Ez egy SDL2 alapú boids/flocking szimuláció. A világ toroid módon viselkedik, tehát a képernyő egyik oldalán kilépő objektum a másik oldalon jelenik meg újra. A program futtatható soros módban és POSIX pthreades módban is, így a két megoldás sebessége közvetlenül összehasonlítható.

A párhuzamosítás lényege egyszerű: a boid tömb index-tartományokra van felosztva, és minden worker thread a saját szeletét számolja ki. Ennek a kódja főleg a `src/update_pthreads.c` fájlban van.

## Játékmódok

- `peaceful`: alap flocking szimuláció
- `survival`: a játékost ragadozók üldözik, van HP és túlélési pontszám
- `terminate44`: a cél minél több boid elkapása, külön kill számlálóval

## Irányítás

- `WASD`: mozgás
- `SPACE`: shockwave
- `TAB`: jobb felső stat panel ki- és bekapcsolása
- bal egérgomb: játékmód kiválasztása a bal felső menüből
- `Q` vagy `ESC`: kilépés

## Fordítás

```powershell
.\make.cmd
```

Indításkor feljön egy kis ablak, ahol elég megadni a boidok számát és a szálak számát. Ha a szálak száma 1, akkor soros futás indul, ha nagyobb, akkor pthreades futás.

Parancssori indítás továbbra is használható, ha valaki kézzel akar konfigurálni:

```powershell
\.\boids_pthreads.exe --mode pthread --threads 4 --boids 800

## Mérés

A mérésekhez a `boids_benchmark.exe` használata ajánlott. Ez külön futtatható, induláskor feljön egy ablak, ahol megadható a boidszám, a szálak száma és az összehasonlítás lépésszáma.

A benchmark futás menete:

- indulás előtt lefut egy soros vs pthread összehasonlítás
- utána elindul maga a játék élőben
- a program futás közben másodpercenként írja a mérési adatokat a konzolba
- bezáráskor a mért adatok egy `benchmark_session_*.txt` fájlba is bekerülnek

## Fontos fájlok

- `src/boids.c`: a flocking szabályok és a világ frissítése
- `src/update_pthreads.c`: a pthread worker szálak és a szeletelt párhuzamos számolás
- `src/main.c`: SDL ablakkezelés, játékmódok, HUD, benchmark parancssor

Assets:
https://drive.google.com/file/d/11_eZSG3nhjnC1REgyEiaBI7g_qronA4c/view?usp=sharing

