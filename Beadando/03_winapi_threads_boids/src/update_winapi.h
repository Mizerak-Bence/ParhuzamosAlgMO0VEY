#pragma once

#include "boids.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct UpdateWinApi UpdateWinApi;

struct UpdateWinApi {
    size_t threadCount;
    void* impl;
};

bool update_winapi_init(UpdateWinApi* u, size_t threadCount);
void update_winapi_destroy(UpdateWinApi* u);
void update_winapi_step(UpdateWinApi* u, World* w, double dt);
