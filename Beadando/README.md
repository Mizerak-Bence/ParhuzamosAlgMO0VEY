# Boids beadandók

Ez a mappa három SDL2 alapú boids/flocking beadandót tartalmaz. Mindhárom projekt ugyanarra az alapötletre épül: a boidok mozgását a cohesion, alignment, separation és a játékos elkerülése határozza meg, a különbség a párhuzamosítás módjában van.

## Szükséges környezet

Ahhoz, hogy bármelyik projektet le lehessen fordítani és futtatni, a következők kellenek:

- Windowsos környezet
- `gcc` vagy más MinGW-w64 alapú GCC toolchain
- `mingw32-make` vagy a projektekben lévő `make.cmd` wrapper
- `pkg-config`
- SDL2 fejlesztői csomag, tehát header és library is

A legegyszerűbb út általában egy MSYS2 vagy MinGW-w64 alapú környezet. MSYS2 esetén a tipikus telepítendő csomagok:

```powershell
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-pkgconf mingw-w64-x86_64-SDL2
```

Megjegyzések a három feladathoz:

- A `01_pthreads_boids` a `-pthread` kapcsolót használja. MinGW-w64 alatt ehhez a szükséges `winpthreads` támogatás jellemzően a toolchain része.
- A `02_openmp_boids` a `-fopenmp` kapcsolót használja. GCC alatt az OpenMP támogatás általában külön extra telepítés nélkül elérhető.
- A `03_winapi_threads_boids` WinAPI threadeket használ, ezért ez a változat Windowsra van kihegyezve. A WinAPI nem külön telepítendő könyvtár, a fordítónak kell tudnia a Windows célplatformot.

Ha a `pkg-config` nem találja az SDL2-t, akkor a Makefile-ban is látható módon kézzel is megadható az include és a library útvonal az `SDL2_CFLAGS` és `SDL2_LDFLAGS` változókon keresztül.

PowerShell alatt a legbiztosabb fordítási parancs minden projektben a helyi `make.cmd`, tehát a példákban ezt használjuk.

## Projektek

### 01 – POSIX pthreads: Boids / Flocking

Ez a projekt egy SDL2 alapú boids szimuláció pthreades változata. A cél ugyanazt a világfrissítést soros és POSIX pthreades módban futtatni, hogy a két megoldás közvetlenül összehasonlítható legyen.

- Cél: a boidok frissítésének párhuzamosítása worker szálak között felosztott index-tartományokkal
- Fókusz: a pthreades update réteg és a soros referencia változat összevetése
- Jellemzők: SDL2 megjelenítés, játékosvezérlés, több játékmód, külön benchmark futtatás
- Mappa: `01_pthreads_boids`

Fordítás:

```powershell
cd ".\01_pthreads_boids"
.\make.cmd
```

Futtatás:

```powershell
.\boids_pthreads.exe
```

Benchmark:

```powershell
.\boids_benchmark.exe
```

Részletesebb leírás: `01_pthreads_boids/README.md`

## 02 – OpenMP: Boids / Flocking

Ez a boids világfrissítés OpenMP-s változata. A flocking logika ugyanaz marad, a különbség az, hogy a világfrissítés ciklusai OpenMP-vel vannak felosztva a szálak között.

- Cél: a boids frissítés gyorsítása OpenMP-vel
- Fókusz: a szekvenciális logika megtartása mellett a fő update ciklusok párhuzamosítása
- Jellemzők: SDL2-es futtatás, indító prompt, külön benchmark futtatható állomány, soros vs OpenMP összehasonlítás
- Mappa: `02_openmp_boids`

Fordítás:

```powershell
cd ".\02_openmp_boids"
.\make.cmd
```

Futtatás:

```powershell
.\boids_openmp.exe
```

Benchmark:

```powershell
.\boids_openmp_benchmark.exe
```

Részletesebb leírás: `02_openmp_boids/README.md`

## 03 – Windows threads (WinAPI): Boids / Flocking

Ez a projekt a boids szimuláció WinAPI threades változata. A cél a 01-es pthreades megoldás logikáját Windowsos szálkezeléssel megvalósítani, például `CreateThread`, eventek és `WaitFor*` hívások használatával.

- Cél: a 01-es feladat Windows threades újraimplementálása
- Fókusz: a párhuzamosítás WinAPI rétege, miközben a boids logika és a benchmark jelleg megmarad
- Jellemzők: SDL2 megjelenítés, külön benchmark futtatható állomány, soros vs WinAPI összehasonlítás
- Mappa: `03_winapi_threads_boids`

Fordítás:

```powershell
cd ".\03_winapi_threads_boids"
.\make.cmd
```

Futtatás:

```powershell
.\boids_winapi.exe
```

Benchmark:

```powershell
.\boids_winapi_benchmark.exe
```

Részletesebb leírás: `03_winapi_threads_boids/README.md`

## Megjegyzés az assetekről

Az első beadandó használ külön asseteket is. Ezek a projekt saját mappájában vagy a mellette lévő erőforrásmappákban vannak, ezért futtatás előtt érdemes megnézni az adott projekt README-ját is, ha valamelyik grafikai elem hiányzik.

## Jatekmodok

- `peaceful`: alap flocking szimulacio
- `survival`: a jatekost ragadozok uldozik, van HP es tulelesi pontszam
- `terminate44`: a cel minel tobb boid elkapasa, kulon kill szamlaloval

## Iranyitas

- `WASD`: mozgas
- `SPACE`: shockwave
- `TAB`: jobb felso stat panel ki- es bekapcsolasa
- bal egérgomb: jatekmod kivalasztasa a bal felso menubol
- `Q` vagy `ESC`: kilepes
