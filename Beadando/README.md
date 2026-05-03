## Projektek

### 01 – POSIX pthreads: Boids / Flocking
- Cél: ugyanazt a boids szimulációt futtatni soros és `pthreads` módban, így a két megoldás közvetlenül összehasonlítható.
- Fókusz: a boidok frissítésének szeletelt, worker szálak közti felosztása POSIX threadekkel.
- Jellemzők: SDL2-megjelenítés, játékosvezérlés, több játékmód, benchmark futtatás.

### 02 – OpenMP: Boids / Flocking
- Cél: a boids világfrissítés OpenMP-s párhuzamosítása.
- Fókusz: a szekvenciális boids-logika megtartása mellett az update ciklusok OpenMP-vel való gyorsítása.
- Jellemzők: SDL2-es futtatás, indító prompt, külön benchmark futtatható állomány, soros vs OpenMP összevetés.

### 03 – Windows threads (WinAPI): Boids / Flocking
- Cél: a 01-es boids feladat WinAPI threadekkel (`CreateThread`, eventek, `WaitFor*`) megvalósított változata.
- Fókusz: a pthreades megoldás logikájának átültetése Windowsos szálkezelésre.
