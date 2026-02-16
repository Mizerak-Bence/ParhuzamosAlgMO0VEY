#pragma once

#include "boids.h"

typedef struct Renderer {
    int width;
    int height;
} Renderer;

void renderer_init(Renderer* r, int width, int height);
void renderer_shutdown(Renderer* r);
void renderer_draw_world(Renderer* r, const World* w, double avgMs, int threads);
