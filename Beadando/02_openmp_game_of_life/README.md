# 02 – OpenMP: Conway Game of Life

## Cél
Conway Game of Life rácsfrissítés OpenMP-vel (`#pragma omp parallel for`), és speedup mérése.

## Input
- WASD: kurzor mozgatása
- Space: cella toggle
- P: pause/run
- Q: kilépés

## Build/Run (MinGW)
- `make`
- `make run`

## Copy-paste (PowerShell) – gcc build + run
Fordítás:
```powershell
cd "d:\Egyetem\2025_26_2\Párhuzamos algoritmusok\Beadando\02_openmp_game_of_life"
gcc -O2 -std=c11 -Wall -Wextra -Wpedantic src\*.c -o life_openmp.exe -fopenmp
```

Futtatás:
```powershell
./life_openmp.exe --threads 4 --width 80 --height 25
```
Kilépés: `Q`

Indítás külön konzolablakban:
```powershell
Start-Process cmd -ArgumentList '/k', 'cd /d "d:\Egyetem\2025_26_2\Párhuzamos algoritmusok\Beadando\02_openmp_game_of_life" && life_openmp.exe --threads 4 --width 80 --height 25'
```

## Mérés
- `--threads N` (OpenMP: `omp_set_num_threads`)
- `--width W --height H`
- átlag ms/step
