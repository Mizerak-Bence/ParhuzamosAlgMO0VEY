#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct AppConfig {
    int width;
    int height;
    int threads;
} AppConfig;

typedef struct Life {
    int width;
    int height;
    uint8_t* a;
    uint8_t* b;
} Life;

bool life_init(Life* life, int width, int height);
void life_destroy(Life* life);

void life_toggle(Life* life, int x, int y);
void life_seed_glider(Life* life, int x, int y);

double life_step_openmp(Life* life, int threads);
