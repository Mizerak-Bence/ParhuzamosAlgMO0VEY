#include "boids.h"
#include "update_pthreads.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__has_include)
#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#elif __has_include(<SDL.h>)
#include <SDL.h>
#else
#error "SDL2 header not found. Install SDL2 dev package and set SDL2_CFLAGS in Makefile."
#endif
#else
#include <SDL2/SDL.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

typedef enum RunMode {
    RUNMODE_SEQ = 0,
    RUNMODE_PTHREAD = 1,
} RunMode;

typedef struct AppConfig {
    int width;
    int height;
    int boidCount;
    int threadCount;
    RunMode mode;
} AppConfig;

static void print_usage(const char* exe) {
    printf("Usage: %s [--mode seq|pthread] [--threads N] [--boids N] [--width W] [--height H]\n", exe);
    printf("Controls (in window): WASD move player, Q or ESC quit\n");
}

static int parse_int(const char* s, int defaultValue) {
    if (s == NULL) return defaultValue;
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s) return defaultValue;
    return (int)v;
}

static uint64_t time_now_us(void) {
    const uint64_t c = (uint64_t)SDL_GetPerformanceCounter();
    const uint64_t f = (uint64_t)SDL_GetPerformanceFrequency();
    if (f == 0) return 0;
    return (c * 1000000ULL) / f;
}

static void time_sleep_us(uint64_t us) {
    if (us == 0) return;
    uint32_t ms = (uint32_t)(us / 1000ULL);
    if (ms == 0) ms = 1;
    SDL_Delay(ms);
}

typedef struct AppState {
    AppConfig cfg;
    World world;
    UpdatePthreads updater;
    bool updaterInited;

    int baseWorldW;
    int baseWorldH;
    float targetPixelsPerUnit;

    InputState input;
    bool quit;

    double avgMs;
    int avgCount;

    SDL_Window* window;
    SDL_Renderer* renderer;
    int winW;
    int winH;
    int titleCounter;
} AppState;

static void app_update_world_bounds_for_window(AppState* s) {
    if (!s || !s->window) return;
    if (s->targetPixelsPerUnit <= 0.5f) s->targetPixelsPerUnit = 10.0f;

    int cw = 0, ch = 0;
    SDL_GetWindowSize(s->window, &cw, &ch);
    if (cw <= 1 || ch <= 1) return;

    int drawW = cw;
    int drawH = ch;

    int desiredW = (int)((float)drawW / s->targetPixelsPerUnit);
    int desiredH = (int)((float)drawH / s->targetPixelsPerUnit);
    if (desiredW < s->baseWorldW) desiredW = s->baseWorldW;
    if (desiredH < s->baseWorldH) desiredH = s->baseWorldH;
    if (desiredW < 10) desiredW = 10;
    if (desiredH < 10) desiredH = 10;

    if (s->world.width != desiredW || s->world.height != desiredH) {
        s->world.width = desiredW;
        s->world.height = desiredH;
        if (s->world.player.pos.x > (float)(s->world.width - 1)) s->world.player.pos.x = (float)(s->world.width - 1);
        if (s->world.player.pos.y > (float)(s->world.height - 1)) s->world.player.pos.y = (float)(s->world.height - 1);
        if (s->world.player.pos.x < 0) s->world.player.pos.x = 0;
        if (s->world.player.pos.y < 0) s->world.player.pos.y = 0;
    }
}

static void input_set_key(InputState* in, SDL_Keycode key, bool down) {
    if (key == SDLK_w) in->up = down;
    if (key == SDLK_s) in->down = down;
    if (key == SDLK_a) in->left = down;
    if (key == SDLK_d) in->right = down;
}

static void draw_filled_circle(SDL_Renderer* r, int cx, int cy, int radius) {
    if (radius <= 0) return;
    for (int y = -radius; y <= radius; y++) {
        int x = (int)sqrtf((float)(radius * radius - y * y));
        SDL_RenderDrawLine(r, cx - x, cy + y, cx + x, cy + y);
    }
}

static void draw_world_sdl(AppState* s) {
    if (!s || !s->renderer || !s->window) return;

    SDL_GetWindowSize(s->window, &s->winW, &s->winH);
    app_update_world_bounds_for_window(s);

    SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255);
    SDL_RenderClear(s->renderer);

    const int drawW = s->winW;
    const int drawH = s->winH;
    const float sx = (s->world.width > 0) ? ((float)drawW / (float)(s->world.width)) : 1.0f;
    const float sy = (s->world.height > 0) ? ((float)drawH / (float)(s->world.height)) : 1.0f;
    float scale = sx < sy ? sx : sy;
    if (scale < 1.0f) scale = 1.0f;

    const float worldPixW = (float)s->world.width * scale;
    const float worldPixH = (float)s->world.height * scale;
    const float ox = 0.5f * ((float)drawW - worldPixW);
    const float oy = 0.5f * ((float)drawH - worldPixH);

    float triSize = 6.0f;
    if (scale > 0.5f) triSize = 0.6f * scale;
    if (triSize < 4.0f) triSize = 4.0f;
    if (triSize > 14.0f) triSize = 14.0f;

    SDL_SetRenderDrawColor(s->renderer, 200, 200, 255, 255);
    for (size_t i = 0; i < s->world.boidCount; i++) {
        const Boid* b = &s->world.boids[i];
        float vx = b->vel.x;
        float vy = b->vel.y;
        float vl = sqrtf(vx * vx + vy * vy);
        if (vl < 1e-4f) { vx = 1.0f; vy = 0.0f; vl = 1.0f; }
        vx /= vl;
        vy /= vl;

        float px = ox + b->pos.x * scale;
        float py = oy + b->pos.y * scale;
        float hx = px + vx * triSize;
        float hy = py + vy * triSize;
        float pxp = -vy;
        float pyp = vx;
        float bx = px - vx * (triSize * 0.7f);
        float by = py - vy * (triSize * 0.7f);
        float lx = bx + pxp * (triSize * 0.6f);
        float ly = by + pyp * (triSize * 0.6f);
        float rx = bx - pxp * (triSize * 0.6f);
        float ry = by - pyp * (triSize * 0.6f);

        SDL_Point pts[4];
        pts[0] = (SDL_Point){(int)(hx + 0.5f), (int)(hy + 0.5f)};
        pts[1] = (SDL_Point){(int)(lx + 0.5f), (int)(ly + 0.5f)};
        pts[2] = (SDL_Point){(int)(rx + 0.5f), (int)(ry + 0.5f)};
        pts[3] = pts[0];
        SDL_RenderDrawLines(s->renderer, pts, 4);
    }

    SDL_SetRenderDrawColor(s->renderer, 220, 60, 60, 255);
    int ppx = (int)(ox + s->world.player.pos.x * scale + 0.5f);
    int ppy = (int)(oy + s->world.player.pos.y * scale + 0.5f);
    int pr = (int)(triSize * 0.7f);
    draw_filled_circle(s->renderer, ppx, ppy, pr);

    SDL_RenderPresent(s->renderer);
}

static void update_window_title(AppState* s) {
    if (!s || !s->window) return;
    if (++s->titleCounter < 12) return;
    s->titleCounter = 0;
    char title[256];
    snprintf(title, sizeof(title),
             "Boids=%zu | mode=%s | threads=%d | avg=%.3f ms/tick | WASD | Q/ESC quit",
             s->world.boidCount,
             s->cfg.mode == RUNMODE_SEQ ? "seq" : "pthread",
             s->cfg.threadCount,
             s->avgMs);
    SDL_SetWindowTitle(s->window, title);
}

int main(int argc, char** argv) {
    AppConfig cfg = {
        .width = 80,
        .height = 25,
        .boidCount = 200,
        .threadCount = 4,
        .mode = RUNMODE_PTHREAD,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            const char* m = argv[++i];
            if (strcmp(m, "seq") == 0) cfg.mode = RUNMODE_SEQ;
            else if (strcmp(m, "pthread") == 0) cfg.mode = RUNMODE_PTHREAD;
            else {
                fprintf(stderr, "Unknown mode: %s\n", m);
                return 2;
            }
            continue;
        }
        if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) { cfg.threadCount = parse_int(argv[++i], cfg.threadCount); continue; }
        if (strcmp(argv[i], "--boids") == 0 && i + 1 < argc) { cfg.boidCount = parse_int(argv[++i], cfg.boidCount); continue; }
        if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) { cfg.width = parse_int(argv[++i], cfg.width); continue; }
        if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) { cfg.height = parse_int(argv[++i], cfg.height); continue; }

        fprintf(stderr, "Unknown arg: %s\n", argv[i]);
        print_usage(argv[0]);
        return 2;
    }

    if (cfg.width <= 10 || cfg.height <= 10 || cfg.boidCount <= 0 || cfg.threadCount <= 0) {
        fprintf(stderr, "Invalid config. Use --help\n");
        return 2;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    srand((unsigned)time_now_us());

    AppState st = {0};
    st.cfg = cfg;
    st.baseWorldW = cfg.width;
    st.baseWorldH = cfg.height;
    st.targetPixelsPerUnit = 10.0f;

    if (!world_init(&st.world, cfg.width, cfg.height, (size_t)cfg.boidCount)) {
        fprintf(stderr, "world_init failed\n");
        return 1;
    }

    if (cfg.mode == RUNMODE_PTHREAD) {
        if (!update_pthreads_init(&st.updater, (size_t)cfg.threadCount)) {
            fprintf(stderr, "update_pthreads_init failed\n");
            world_destroy(&st.world);
            return 1;
        }
        st.updaterInited = true;
    }

    const int scale = 10;
    int winW = cfg.width * scale;
    int winH = cfg.height * scale;

    st.window = SDL_CreateWindow(
        "Boids (SDL2)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        winW,
        winH,
        SDL_WINDOW_RESIZABLE);

    if (!st.window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        if (st.updaterInited) update_pthreads_destroy(&st.updater);
        world_destroy(&st.world);
        SDL_Quit();
        return 1;
    }

    st.renderer = SDL_CreateRenderer(st.window, -1, SDL_RENDERER_ACCELERATED);
    if (!st.renderer) {
        st.renderer = SDL_CreateRenderer(st.window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!st.renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(st.window);
        if (st.updaterInited) update_pthreads_destroy(&st.updater);
        world_destroy(&st.world);
        SDL_Quit();
        return 1;
    }

    const double simDt = 1.0 / 120.0;
    double acc = 0.0;
    uint64_t lastUs = time_now_us();

    while (!st.quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                st.quit = true;
                break;
            }
            if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                const bool down = (e.type == SDL_KEYDOWN);
                SDL_Keycode key = e.key.keysym.sym;
                input_set_key(&st.input, key, down);
                if (down && (key == SDLK_ESCAPE || key == SDLK_q)) {
                    st.quit = true;
                    break;
                }
            }
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                app_update_world_bounds_for_window(&st);
            }
        }
        if (st.quit) break;

        uint64_t nowUs = time_now_us();
        double frameDt = (double)(nowUs - lastUs) / 1000000.0;
        lastUs = nowUs;
        if (frameDt > 0.05) frameDt = 0.05;
        acc += frameDt;

        while (acc >= simDt) {
            app_update_world_bounds_for_window(&st);
            world_apply_player_input(&st.world, &st.input, simDt);

            const uint64_t t0 = time_now_us();
            if (cfg.mode == RUNMODE_SEQ) {
                Vec2 centroid = world_compute_centroid(&st.world);
                float detachDist2 = world_compute_detachDist2(&st.world);
                world_compute_detached_range(&st.world, centroid, detachDist2, st.world.detached, 0, st.world.boidCount);
                world_step_range(&st.world, &st.world, 0, st.world.boidCount, simDt, st.world.detached);
                world_swap_buffers(&st.world);
            } else {
                update_pthreads_step(&st.updater, &st.world, simDt);
            }
            const uint64_t t1 = time_now_us();
            const double ms = (double)(t1 - t0) / 1000.0;
            st.avgCount++;
            st.avgMs += (ms - st.avgMs) / (double)st.avgCount;

            acc -= simDt;
        }

        update_window_title(&st);
        draw_world_sdl(&st);
        time_sleep_us(1000);
    }

    if (st.renderer) SDL_DestroyRenderer(st.renderer);
    if (st.window) SDL_DestroyWindow(st.window);
    if (st.updaterInited) update_pthreads_destroy(&st.updater);
    world_destroy(&st.world);
    SDL_Quit();
    return 0;
}
