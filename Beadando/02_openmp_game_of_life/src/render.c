#include "render.h"

#include <stdio.h>
#include <stdlib.h>

static void clear_screen(void) {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H");
#endif
}

static int idx(const Life* life, int x, int y) {
    return y * life->width + x;
}

void renderer_init(Renderer* r, int width, int height) {
    r->width = width;
    r->height = height;
}

void renderer_shutdown(Renderer* r) {
    (void)r;
}

void renderer_draw(Renderer* r, const Life* life, int cursorX, int cursorY, bool running, int threads, double stepMs) {
    (void)r;
    clear_screen();

    printf("Game of Life | OpenMP threads=%d | %s | step=%.3f ms\n", threads, running ? "RUN" : "PAUSE", stepMs);
    printf("Controls: WASD move cursor, SPACE toggle, P pause, Q quit\n\n");

    for (int y = 0; y < life->height; y++) {
        for (int x = 0; x < life->width; x++) {
            char c = life->a[idx(life, x, y)] ? '#' : '.';
            if (x == cursorX && y == cursorY) c = life->a[idx(life, x, y)] ? 'X' : 'O';
            putchar(c);
        }
        putchar('\n');
    }
}
