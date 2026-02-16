# Beadandók (Párhuzamos algoritmusok)

## 01 – POSIX pthreads: Boids/Flocking
- Cél: ugyanazt a boids szimulációt futtatni soros és `pthreads` párhuzamos módban.
- Input: a player WASD-vel mozog (nem scripted), a boidok autonómok.

## 02 – OpenMP: Conway Game of Life + interaktív szerkesztés
- Cél: Game of Life rács frissítése OpenMP-vel.
- Input: kurzor WASD-vel, cella toggle pl. `space`, fut/pause.

## 03 – Windows threads (WinAPI): Boids/Flocking + player input
- Cél: a 01-es boids feladat WinAPI thread-ekkel (CreateThread/WaitFor*) párhuzamosítva.

## Fordítás (MinGW)
Mindhárom projekt külön `Makefile`-t kap.
