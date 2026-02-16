# Beadandók (Párhuzamos algoritmusok)

Ez a mappa 3 külön beadandót tartalmaz, mindegyik C-ben, több fájlra bontva, globális változók minimalizálásával.

## 01 – POSIX pthreads: Boids/Flocking + player input
- Cél: ugyanazt a boids szimulációt futtatni soros és `pthreads` párhuzamos módban.
- Mérni: futási idő / tick, speedup különböző szál-számokkal.
- Input: a player WASD-vel mozog (nem scripted), a boidok autonómok.

## 02 – OpenMP: Conway Game of Life + interaktív szerkesztés
- Cél: Game of Life rács frissítése OpenMP-vel.
- Input: kurzor WASD-vel, cella toggle pl. `space`, fut/pause.
- Mérni: frissítési idő, speedup a rácsmérettől és szál-számtól függően.

## 03 – Windows threads (WinAPI): Boids/Flocking + player input
- Cél: a 01-es boids feladat WinAPI thread-ekkel (CreateThread/WaitFor*) párhuzamosítva.
- Mérni: idő / tick, speedup, összehasonlítás a pthreads verzióval.

## Fordítás (MinGW)
Mindhárom projekt külön `Makefile`-t kap.

Windows/MinGW megjegyzés:
- PowerShell-ben: `mingw32-make` (vagy `./make.cmd`)
- CMD-ben elég: `make` (a mappákban van `make.cmd` wrapper)

Általános:
- `make`
- `make run`
- `make clean`

