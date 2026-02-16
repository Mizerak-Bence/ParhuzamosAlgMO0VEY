#pragma once

#include "life.h"

typedef struct Renderer {
    int width;
    int height;
} Renderer;

void renderer_init(Renderer* r, int width, int height);
void renderer_shutdown(Renderer* r);
void renderer_draw(Renderer* r, const Life* life, int cursorX, int cursorY, bool running, int threads, double stepMs);
