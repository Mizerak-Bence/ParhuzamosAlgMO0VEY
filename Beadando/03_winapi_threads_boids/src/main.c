#include "boids.h"
#include "update_winapi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>

#if defined(__has_include)
#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#elif __has_include(<SDL.h>)
#include <SDL.h>
#else
#if defined(__INTELLISENSE__)
typedef unsigned char Uint8;
typedef unsigned int Uint32;
typedef unsigned long long Uint64;

typedef int SDL_Keycode;

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;

typedef struct SDL_Keysym {
    SDL_Keycode sym;
} SDL_Keysym;

typedef struct SDL_KeyboardEvent {
    SDL_Keysym keysym;
} SDL_KeyboardEvent;

typedef struct SDL_Event {
    Uint32 type;
    SDL_KeyboardEvent key;
} SDL_Event;

#define SDL_INIT_VIDEO 0x00000020u
#define SDL_QUIT 0x00000100u
#define SDL_KEYDOWN 0x00000300u
#define SDL_KEYUP 0x00000301u

#define SDLK_ESCAPE 27
#define SDLK_w 'w'
#define SDLK_a 'a'
#define SDLK_s 's'
#define SDLK_d 'd'
#define SDLK_q 'q'

#define SDL_WINDOWPOS_CENTERED 0
#define SDL_WINDOW_RESIZABLE 0x00000020u

#define SDL_RENDERER_SOFTWARE 0x00000001u
#define SDL_RENDERER_ACCELERATED 0x00000002u

Uint64 SDL_GetPerformanceCounter(void);
Uint64 SDL_GetPerformanceFrequency(void);
void SDL_Delay(Uint32 ms);

int SDL_Init(Uint32 flags);
void SDL_Quit(void);
const char* SDL_GetError(void);

SDL_Window* SDL_CreateWindow(const char* title, int x, int y, int w, int h, Uint32 flags);
SDL_Renderer* SDL_CreateRenderer(SDL_Window* window, int index, Uint32 flags);
void SDL_DestroyRenderer(SDL_Renderer* renderer);
void SDL_DestroyWindow(SDL_Window* window);
void SDL_GetWindowSize(SDL_Window* window, int* w, int* h);
int SDL_PollEvent(SDL_Event* event);
void SDL_SetWindowTitle(SDL_Window* window, const char* title);

int SDL_SetRenderDrawColor(SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int SDL_RenderClear(SDL_Renderer* renderer);
int SDL_RenderDrawLine(SDL_Renderer* renderer, int x1, int y1, int x2, int y2);
void SDL_RenderPresent(SDL_Renderer* renderer);
#else
#error "SDL2 header not found. Install SDL2 dev package and set SDL2_CFLAGS in Makefile."
#endif
#endif
#else
#include <SDL2/SDL.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

typedef struct AppConfig {
    int width;
    int height;
    int boidCount;
    int threadCount;
    int stepsPerSecond;
} AppConfig;

typedef struct Input {
    InputState held;
} Input;

typedef struct Renderer {
    SDL_Window* window;
    SDL_Renderer* renderer;
    int winW;
    int winH;
} Renderer;

static void usage(const char* exe) {
    printf("Usage: %s [--threads N] [--boids N] [--width W] [--height H]\n", exe);
}

static int parse_int(const char* s, int def) {
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s) return def;
    return (int)v;
}

static uint64_t time_now_us(void) {
    static LARGE_INTEGER freq;
    static int init = 0;
    LARGE_INTEGER now;

    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = 1;
    }

    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart * 1000000ULL) / (uint64_t)freq.QuadPart);
}

static void time_sleep_us(uint64_t us) {
    DWORD ms;

    if (us == 0) return;
    ms = (DWORD)(us / 1000ULL);
    if (ms == 0) ms = 1;
    Sleep(ms);
}

static void input_init(Input* input) {
    memset(input, 0, sizeof(*input));
}

static InputState input_poll(Input* input) {
    SDL_Event e;

    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            input->held.quit = true;
        } else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            const bool down = e.type == SDL_KEYDOWN;
            SDL_Keycode key = e.key.keysym.sym;

            if ((key == SDLK_ESCAPE || key == SDLK_q) && down) input->held.quit = true;
            if (key == SDLK_w) input->held.up = down;
            if (key == SDLK_s) input->held.down = down;
            if (key == SDLK_a) input->held.left = down;
            if (key == SDLK_d) input->held.right = down;
        }
    }

    return input->held;
}

static void draw_rect_outline(SDL_Renderer* renderer, int x, int y, int w, int h) {
    SDL_RenderDrawLine(renderer, x, y, x + w, y);
    SDL_RenderDrawLine(renderer, x + w, y, x + w, y + h);
    SDL_RenderDrawLine(renderer, x + w, y + h, x, y + h);
    SDL_RenderDrawLine(renderer, x, y + h, x, y);
}

static void draw_rect_filled(SDL_Renderer* renderer, int x, int y, int w, int h) {
    for (int yy = 0; yy <= h; yy++) {
        SDL_RenderDrawLine(renderer, x, y + yy, x + w, y + yy);
    }
}

static void renderer_update_metrics(Renderer* r) {
    SDL_GetWindowSize(r->window, &r->winW, &r->winH);
}

static void renderer_update_title(Renderer* r, const World* w, double avgMs, int threads) {
    char title[256];

    snprintf(title, sizeof(title),
             "Boids (WinAPI)=%zu | threads=%d | avg=%.3f ms/tick | WASD move | Q quit",
             w->boidCount,
             threads,
             avgMs);
    SDL_SetWindowTitle(r->window, title);
}

static bool renderer_init(Renderer* r, int width, int height) {
    int winW = width * 12;
    int winH = height * 12;

    memset(r, 0, sizeof(*r));

    if (winW < 720) winW = 720;
    if (winH < 520) winH = 520;

    r->window = SDL_CreateWindow("Boids (WinAPI)",
                                 SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED,
                                 winW,
                                 winH,
                                 SDL_WINDOW_RESIZABLE);
    if (!r->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    r->renderer = SDL_CreateRenderer(r->window, -1, SDL_RENDERER_ACCELERATED);
    if (!r->renderer) {
        r->renderer = SDL_CreateRenderer(r->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!r->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(r->window);
        r->window = NULL;
        return false;
    }

    renderer_update_metrics(r);
    return true;
}

static void renderer_shutdown(Renderer* r) {
    if (r->renderer) SDL_DestroyRenderer(r->renderer);
    if (r->window) SDL_DestroyWindow(r->window);
    memset(r, 0, sizeof(*r));
}

static void renderer_draw_world(Renderer* r, const World* w, double avgMs, int threads) {
    int cellSize;
    int boardW;
    int boardH;
    int offsetX;
    int offsetY;
    int boidSize;
    int playerSize;

    renderer_update_metrics(r);
    renderer_update_title(r, w, avgMs, threads);

    cellSize = r->winW / w->width;
    if (r->winH / w->height < cellSize) cellSize = r->winH / w->height;
    if (cellSize < 2) cellSize = 2;

    boardW = cellSize * w->width;
    boardH = cellSize * w->height;
    offsetX = (r->winW - boardW) / 2;
    offsetY = (r->winH - boardH) / 2;
    boidSize = cellSize / 2;
    if (boidSize < 3) boidSize = 3;
    playerSize = cellSize / 2 + 2;
    if (playerSize < 5) playerSize = 5;

    SDL_SetRenderDrawColor(r->renderer, 18, 20, 24, 255);
    SDL_RenderClear(r->renderer);

    SDL_SetRenderDrawColor(r->renderer, 30, 34, 42, 255);
    draw_rect_filled(r->renderer, offsetX, offsetY, boardW, boardH);

    SDL_SetRenderDrawColor(r->renderer, 120, 200, 255, 255);
    for (size_t i = 0; i < w->boidCount; i++) {
        int x = offsetX + (int)(w->boids[i].pos.x * (float)cellSize) - boidSize / 2;
        int y = offsetY + (int)(w->boids[i].pos.y * (float)cellSize) - boidSize / 2;
        draw_rect_filled(r->renderer, x, y, boidSize, boidSize);
    }

    SDL_SetRenderDrawColor(r->renderer, 255, 210, 80, 255);
    draw_rect_filled(r->renderer,
                     offsetX + (int)(w->player.pos.x * (float)cellSize) - playerSize / 2,
                     offsetY + (int)(w->player.pos.y * (float)cellSize) - playerSize / 2,
                     playerSize,
                     playerSize);

    SDL_SetRenderDrawColor(r->renderer, 90, 96, 110, 255);
    draw_rect_outline(r->renderer, offsetX, offsetY, boardW, boardH);
    SDL_RenderPresent(r->renderer);
}

static bool app_run(const AppConfig* cfg) {
    World w;
    Input in;
    Renderer r;
    UpdateWinApi upd;
    const double dt = 1.0 / (double)cfg->stepsPerSecond;
    double avgMs = 0.0;
    int frame = 0;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    if (!world_init(&w, cfg->width, cfg->height, (size_t)cfg->boidCount)) {
        fprintf(stderr, "world_init failed\n");
        SDL_Quit();
        return false;
    }

    input_init(&in);
    if (!renderer_init(&r, cfg->width, cfg->height)) {
        world_destroy(&w);
        SDL_Quit();
        return false;
    }

    if (!update_winapi_init(&upd, (size_t)cfg->threadCount)) {
        fprintf(stderr, "update_winapi_init failed\n");
        renderer_shutdown(&r);
        world_destroy(&w);
        SDL_Quit();
        return false;
    }

    while (true) {
        InputState st = input_poll(&in);
        uint64_t t0;
        uint64_t t1;
        double ms;

        if (st.quit) break;

        world_apply_player_input(&w, &st, dt);

        t0 = time_now_us();
        update_winapi_step(&upd, &w, dt);
        t1 = time_now_us();

        ms = (double)(t1 - t0) / 1000.0;
        frame++;
        avgMs += (ms - avgMs) / (double)frame;

        renderer_draw_world(&r, &w, avgMs, cfg->threadCount);
        time_sleep_us((uint64_t)(dt * 1000000.0));
    }

    update_winapi_destroy(&upd);
    renderer_shutdown(&r);
    world_destroy(&w);
    SDL_Quit();
    return true;
}

int main(int argc, char** argv) {
    AppConfig cfg = {
        .width = 80,
        .height = 25,
        .boidCount = 200,
        .threadCount = 4,
        .stepsPerSecond = 30,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) cfg.threadCount = parse_int(argv[++i], cfg.threadCount);
        else if (strcmp(argv[i], "--boids") == 0 && i + 1 < argc) cfg.boidCount = parse_int(argv[++i], cfg.boidCount);
        else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) cfg.width = parse_int(argv[++i], cfg.width);
        else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) cfg.height = parse_int(argv[++i], cfg.height);
        else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    return app_run(&cfg) ? 0 : 1;
}
