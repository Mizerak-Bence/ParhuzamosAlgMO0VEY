#pragma once

#include "boids.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct UpdatePthreads UpdatePthreads;

struct UpdatePthreads {
    size_t threadCount;
    void* impl;
};

bool update_pthreads_init(UpdatePthreads* updater, size_t threadCount);
void update_pthreads_destroy(UpdatePthreads* updater);
void update_pthreads_step(UpdatePthreads* updater, World* world, double dt);
