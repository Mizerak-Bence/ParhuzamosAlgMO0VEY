#include "life.h"

#include "timeutil.h"

#ifdef _OPENMP
#include <omp.h>
#endif
#include <stdlib.h>
#include <string.h>

static int idx(const Life* life, int x, int y) {
    return y * life->width + x;
}

bool life_init(Life* life, int width, int height) {
    memset(life, 0, sizeof(*life));
    life->width = width;
    life->height = height;
    size_t n = (size_t)width * (size_t)height;
    life->a = (uint8_t*)calloc(n, 1);
    life->b = (uint8_t*)calloc(n, 1);
    return life->a && life->b;
}

void life_destroy(Life* life) {
    if (!life) return;
    free(life->a);
    free(life->b);
    memset(life, 0, sizeof(*life));
}

void life_toggle(Life* life, int x, int y) {
    int i = idx(life, x, y);
    life->a[i] = (uint8_t)(!life->a[i]);
}

void life_seed_glider(Life* life, int x, int y) {
    int gx[5] = {x, x + 1, x + 2, x + 2, x + 1};
    int gy[5] = {y, y + 1, y, y + 1, y + 2};
    for (int k = 0; k < 5; k++) {
        int px = gx[k];
        int py = gy[k];
        if (px >= 0 && px < life->width && py >= 0 && py < life->height) {
            life->a[idx(life, px, py)] = 1;
        }
    }
}

static int count_neighbors(const Life* life, int x, int y) {
    int n = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx;
            int ny = y + dy;
            if (nx < 0 || nx >= life->width || ny < 0 || ny >= life->height) continue;
            n += life->a[idx(life, nx, ny)] ? 1 : 0;
        }
    }
    return n;
}

double life_step_openmp(Life* life, int threads) {
#ifdef _OPENMP
    omp_set_num_threads(threads);
#else
    (void)threads;
#endif

    uint64_t t0 = time_now_us();

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int y = 0; y < life->height; y++) {
        for (int x = 0; x < life->width; x++) {
            int n = count_neighbors(life, x, y);
            uint8_t alive = life->a[idx(life, x, y)];
            uint8_t next = 0;
            if (alive) {
                next = (n == 2 || n == 3) ? 1 : 0;
            } else {
                next = (n == 3) ? 1 : 0;
            }
            life->b[idx(life, x, y)] = next;
        }
    }

    uint64_t t1 = time_now_us();

    uint8_t* tmp = life->a;
    life->a = life->b;
    life->b = tmp;

    return (double)(t1 - t0) / 1000.0;
}
