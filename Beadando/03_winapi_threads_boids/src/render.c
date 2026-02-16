#include "render.h"

#include <stdio.h>
#include <stdlib.h>

static void clear_screen(void) {
    system("cls");
}

void renderer_init(Renderer* r, int width, int height) {
    r->width = width;
    r->height = height;
}

void renderer_shutdown(Renderer* r) {
    (void)r;
}

void renderer_draw_world(Renderer* r, const World* w, double avgMs, int threads) {
    (void)r;
    clear_screen();

    printf("Boids (WinAPI)=%zu | threads=%d | avg=%.3f ms/tick\n", w->boidCount, threads, avgMs);
    printf("Controls: WASD move player, Q quit\n\n");

    for (int y = 0; y < w->height; y++) {
        for (int x = 0; x < w->width; x++) {
            char ch = ' ';

            int px = (int)(w->player.pos.x + 0.5f);
            int py = (int)(w->player.pos.y + 0.5f);
            if (x == px && y == py) ch = '@';

            for (size_t i = 0; i < w->boidCount; i++) {
                int bx = (int)(w->boids[i].pos.x + 0.5f);
                int by = (int)(w->boids[i].pos.y + 0.5f);
                if (bx == x && by == y) { ch = 'o'; break; }
            }

            putchar(ch);
        }
        putchar('\n');
    }
}
