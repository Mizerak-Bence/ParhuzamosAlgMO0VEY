#include "boids.h"
#include "update_winapi.h"

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

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

typedef struct SDL_Point {
    int x;
    int y;
} SDL_Point;

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
int SDL_RenderDrawLines(SDL_Renderer* renderer, const SDL_Point* points, int count);
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
    bool benchmarkOnly;
    bool liveBenchmarkSession;
    int benchmarkSteps;
    int runtimeSeconds;
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

typedef struct BenchmarkResult {
    double totalMs;
    double avgMs;
    double ticksPerSecond;
} BenchmarkResult;

typedef struct LiveBenchmarkState {
    bool active;
    double seqAvgMs;
    double winapiAvgMs;
    double speedup;
    double runtimeSec;
    double liveAvgMs;
    double liveTicksPerSecond;
    double totalMsSum;
    int totalTickCount;
    uint64_t startUs;
    uint64_t lastUpdateUs;
    double intervalMsSum;
    int intervalTickCount;
} LiveBenchmarkState;

enum {
    BENCHMARK_WARMUP_TICKS = 100,
};

static FILE* g_benchmarkLog = NULL;
static char g_benchmarkLogPath[260] = {0};

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

static bool exe_name_is_benchmark(const char* exePath) {
    const char* base = exePath;

    if (!exePath) return false;

    for (const char* p = exePath; *p; p++) {
        if (*p == '\\' || *p == '/' || *p == ':') base = p + 1;
    }

    return strstr(base, "benchmark") != NULL;
}

static bool benchmark_open_log_file(void) {
    if (g_benchmarkLog) return true;

    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(g_benchmarkLogPath, sizeof(g_benchmarkLogPath),
             "boids_winapi_benchmark_%04u%02u%02u_%02u%02u%02u.txt",
             (unsigned)st.wYear,
             (unsigned)st.wMonth,
             (unsigned)st.wDay,
             (unsigned)st.wHour,
             (unsigned)st.wMinute,
             (unsigned)st.wSecond);

    g_benchmarkLog = fopen(g_benchmarkLogPath, "w");
    return g_benchmarkLog != NULL;
}

static void benchmark_close_log_file(void) {
    if (!g_benchmarkLog) return;
    fclose(g_benchmarkLog);
    g_benchmarkLog = NULL;
}

static const char* benchmark_log_path(void) {
    return g_benchmarkLogPath[0] ? g_benchmarkLogPath : NULL;
}

static void benchmark_write_text(const char* text) {
    if (!text) return;
    fputs(text, stdout);
    fflush(stdout);
    if (g_benchmarkLog) {
        fputs(text, g_benchmarkLog);
        fflush(g_benchmarkLog);
    }
}

static void benchmark_printf(const char* fmt, ...) {
    char text[768];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);

    benchmark_write_text(text);
}

static void benchmark_attach_console(void) {
    static bool attached = false;

    if (attached) return;
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        (void)freopen("CONOUT$", "w", stdout);
        (void)freopen("CONOUT$", "w", stderr);
        (void)freopen("CONIN$", "r", stdin);
        attached = true;
    }
}

static void usage(const char* exe) {
    printf("Usage: %s [--threads N] [--boids N] [--width W] [--height H]\n", exe);
    printf("       %s --benchmark N [--compare] [--threads N] [--boids N] [--width W] [--height H]\n", exe);
    printf("       %s --live-benchmark N [--runtime S] [--threads N] [--boids N] [--width W] [--height H]\n", exe);
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

static void renderer_update_metrics(Renderer* renderer) {
    SDL_GetWindowSize(renderer->window, &renderer->winW, &renderer->winH);
}

static Vec2 normalize_or_default(Vec2 v, Vec2 fallback) {
    float len = sqrtf(v.x * v.x + v.y * v.y);
    if (len < 1e-4f) return fallback;
    return (Vec2){v.x / len, v.y / len};
}

static void draw_triangle_boid(SDL_Renderer* renderer, float px, float py, Vec2 dir, float size) {
    Vec2 forward = normalize_or_default(dir, (Vec2){1.0f, 0.0f});
    float sideX = -forward.y;
    float sideY = forward.x;
    float headX = px + forward.x * size;
    float headY = py + forward.y * size;
    float baseX = px - forward.x * (size * 0.7f);
    float baseY = py - forward.y * (size * 0.7f);
    float leftX = baseX + sideX * (size * 0.6f);
    float leftY = baseY + sideY * (size * 0.6f);
    float rightX = baseX - sideX * (size * 0.6f);
    float rightY = baseY - sideY * (size * 0.6f);
    SDL_Point pts[4];

    pts[0] = (SDL_Point){(int)(headX + 0.5f), (int)(headY + 0.5f)};
    pts[1] = (SDL_Point){(int)(leftX + 0.5f), (int)(leftY + 0.5f)};
    pts[2] = (SDL_Point){(int)(rightX + 0.5f), (int)(rightY + 0.5f)};
    pts[3] = pts[0];
    SDL_RenderDrawLines(renderer, pts, 4);
}

static void set_group_color(SDL_Renderer* renderer, unsigned char group, int groupCount) {
    static const Uint8 palette[][3] = {
        {230,  70,  70},
        { 70, 210,  70},
        { 70, 140, 230},
        {230, 200,  70},
        {210,  70, 210},
        { 70, 210, 210},
        {230, 120,  70},
        {140,  70, 230},
        {120, 230,  70},
        {230,  70, 140},
        { 70, 230, 120},
        { 70, 120, 230},
        {230, 160,  90},
        {160, 230,  90},
        { 90, 230, 160},
        {160,  90, 230},
    };
    int idx = 0;

    if (groupCount > 0) idx = (int)group % groupCount;
    idx = idx % (int)(sizeof(palette) / sizeof(palette[0]));
    SDL_SetRenderDrawColor(renderer, palette[idx][0], palette[idx][1], palette[idx][2], 255);
}

static void renderer_update_title(Renderer* renderer,
                                  const World* world,
                                  int threads,
                                  double stepMs,
                                  const LiveBenchmarkState* bm) {
    char title[512];

    if (bm && bm->active) {
        snprintf(title, sizeof(title),
                 "Boids | WinAPI threads=%d | tick=%.3f ms | seq=%.3f | winapi=%.3f | speedup=%.2fx | live=%.3f ms/tick",
                 threads,
                 stepMs,
                 bm->seqAvgMs,
                 bm->winapiAvgMs,
                 bm->speedup,
                 bm->liveAvgMs);
    } else {
        snprintf(title, sizeof(title),
                 "Boids | WinAPI boids=%zu | threads=%d | tick=%.3f ms | WASD move | Q quit",
                 world->boidCount,
                 threads,
                 stepMs);
    }

    SDL_SetWindowTitle(renderer->window, title);
}

static bool renderer_init(Renderer* renderer, int width, int height) {
    int winW = width * 12;
    int winH = height * 12;

    memset(renderer, 0, sizeof(*renderer));

    if (winW < 720) winW = 720;
    if (winH < 520) winH = 520;

    renderer->window = SDL_CreateWindow("Boids | WinAPI",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        winW,
                                        winH,
                                        SDL_WINDOW_RESIZABLE);
    if (!renderer->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    renderer->renderer = SDL_CreateRenderer(renderer->window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer->renderer) {
        renderer->renderer = SDL_CreateRenderer(renderer->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(renderer->window);
        renderer->window = NULL;
        return false;
    }

    renderer_update_metrics(renderer);
    return true;
}

static void renderer_shutdown(Renderer* renderer) {
    if (renderer->renderer) SDL_DestroyRenderer(renderer->renderer);
    if (renderer->window) SDL_DestroyWindow(renderer->window);
    memset(renderer, 0, sizeof(*renderer));
}

static void renderer_draw_world(Renderer* renderer,
                                const World* world,
                                Vec2 playerDir,
                                double stepMs,
                                int threads,
                                const LiveBenchmarkState* bm) {
    int cellSize;
    int boardW;
    int boardH;
    int offsetX;
    int offsetY;
    float triSize;

    renderer_update_metrics(renderer);
    renderer_update_title(renderer, world, threads, stepMs, bm);

    cellSize = renderer->winW / world->width;
    if (renderer->winH / world->height < cellSize) cellSize = renderer->winH / world->height;
    if (cellSize < 2) cellSize = 2;

    boardW = cellSize * world->width;
    boardH = cellSize * world->height;
    offsetX = (renderer->winW - boardW) / 2;
    offsetY = (renderer->winH - boardH) / 2;
    triSize = 0.6f * (float)cellSize;
    if (triSize < 4.0f) triSize = 4.0f;
    if (triSize > 14.0f) triSize = 14.0f;

    SDL_SetRenderDrawColor(renderer->renderer, 18, 20, 24, 255);
    SDL_RenderClear(renderer->renderer);

    SDL_SetRenderDrawColor(renderer->renderer, 30, 34, 42, 255);
    draw_rect_filled(renderer->renderer, offsetX, offsetY, boardW, boardH);

    for (size_t i = 0; i < world->boidCount; i++) {
        const Boid* boid = &world->boids[i];
        float px = (float)offsetX + boid->pos.x * (float)cellSize;
        float py = (float)offsetY + boid->pos.y * (float)cellSize;

        if (!boid->alive) continue;
        if (boid->predator) {
            SDL_SetRenderDrawColor(renderer->renderer, 255, 140, 40, 255);
        } else {
            set_group_color(renderer->renderer, boid->group, world->groupCount);
        }

        draw_triangle_boid(renderer->renderer, px, py, boid->vel, triSize);
    }

    SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 255);
    draw_triangle_boid(renderer->renderer,
                       (float)offsetX + world->player.pos.x * (float)cellSize,
                       (float)offsetY + world->player.pos.y * (float)cellSize,
                       playerDir,
                       triSize);

    SDL_SetRenderDrawColor(renderer->renderer, 90, 96, 110, 255);
    draw_rect_outline(renderer->renderer, offsetX, offsetY, boardW, boardH);
    SDL_RenderPresent(renderer->renderer);
}

static bool setup_world_for_run(World* world, const AppConfig* cfg, unsigned int seed) {
    srand(seed);
    return world_init(world, cfg->width, cfg->height, (size_t)cfg->boidCount);
}

static double world_step_winapi(UpdateWinApi* upd, World* world, double dt) {
    uint64_t t0 = time_now_us();
    update_winapi_step(upd, world, dt);
    return (double)(time_now_us() - t0) / 1000.0;
}

static BenchmarkResult run_benchmark_ticks(const AppConfig* cfg, bool parallel) {
    BenchmarkResult result = {0};
    World world;
    UpdateWinApi upd;
    const double dt = 1.0 / (double)cfg->stepsPerSecond;
    bool updaterInited = false;

    if (!setup_world_for_run(&world, cfg, 1234u)) {
        return result;
    }

    memset(&upd, 0, sizeof(upd));
    if (parallel) {
        if (!update_winapi_init(&upd, (size_t)cfg->threadCount)) {
            world_destroy(&world);
            return result;
        }
        updaterInited = true;
    }

    for (int i = 0; i < BENCHMARK_WARMUP_TICKS; i++) {
        if (parallel) {
            (void)world_step_winapi(&upd, &world, dt);
        } else {
            (void)world_step_seq(&world, dt);
        }
    }

    for (int i = 0; i < cfg->benchmarkSteps; i++) {
        result.totalMs += parallel ? world_step_winapi(&upd, &world, dt)
                                   : world_step_seq(&world, dt);
    }

    if (cfg->benchmarkSteps > 0) {
        result.avgMs = result.totalMs / (double)cfg->benchmarkSteps;
    }
    if (result.totalMs > 0.0) {
        result.ticksPerSecond = (double)cfg->benchmarkSteps * 1000.0 / result.totalMs;
    }

    if (updaterInited) update_winapi_destroy(&upd);
    world_destroy(&world);
    return result;
}

static bool run_compare_benchmark(const AppConfig* cfg, LiveBenchmarkState* bm) {
    BenchmarkResult seqResult;
    BenchmarkResult winapiResult;

    if (cfg->benchmarkSteps <= 0) return false;

    seqResult = run_benchmark_ticks(cfg, false);
    winapiResult = run_benchmark_ticks(cfg, true);

    if (seqResult.avgMs <= 0.0 || winapiResult.avgMs <= 0.0) {
        return false;
    }

    if (bm) {
        memset(bm, 0, sizeof(*bm));
        bm->active = true;
        bm->seqAvgMs = seqResult.avgMs;
        bm->winapiAvgMs = winapiResult.avgMs;
        bm->speedup = seqResult.avgMs / winapiResult.avgMs;
    }

    benchmark_printf("benchmark compare section=boids_step width=%d height=%d boids=%d ticks=%d\n",
                     cfg->width,
                     cfg->height,
                     cfg->boidCount,
                     cfg->benchmarkSteps);
    benchmark_printf("  seq:        avg=%.3f ms/tick | ticks=%.2f/s\n", seqResult.avgMs, seqResult.ticksPerSecond);
    benchmark_printf("  winapi(%d): avg=%.3f ms/tick | ticks=%.2f/s\n", cfg->threadCount, winapiResult.avgMs, winapiResult.ticksPerSecond);
    benchmark_printf("  speedup:    %.2fx\n", seqResult.avgMs / winapiResult.avgMs);
    return true;
}

static void live_benchmark_note_tick(LiveBenchmarkState* bm, double stepMs) {
    if (!bm || !bm->active) return;
    bm->intervalMsSum += stepMs;
    bm->intervalTickCount++;
    bm->totalMsSum += stepMs;
    bm->totalTickCount++;
}

static void live_benchmark_log_if_needed(const AppConfig* cfg,
                                         const World* world,
                                         LiveBenchmarkState* bm,
                                         bool force) {
    uint64_t nowUs;
    uint64_t intervalUs;
    double intervalSec;
    double overallAvgMs = 0.0;

    if (!bm || !bm->active) return;

    nowUs = time_now_us();
    bm->runtimeSec = (double)(nowUs - bm->startUs) / 1000000.0;

    intervalUs = nowUs - bm->lastUpdateUs;
    if (!force && intervalUs < 1000000ULL) return;
    if (bm->intervalTickCount <= 0) return;

    intervalSec = (double)intervalUs / 1000000.0;
    if (intervalSec > 0.0) {
        bm->liveAvgMs = bm->intervalMsSum / (double)bm->intervalTickCount;
        bm->liveTicksPerSecond = (double)bm->intervalTickCount / intervalSec;
    }
    if (bm->totalTickCount > 0) {
        overallAvgMs = bm->totalMsSum / (double)bm->totalTickCount;
    }

    benchmark_printf("[live %.1fs] run=winapi boids=%zu threads=%d interval=%.3f ms/tick overall=%.3f ms/tick ticks=%.2f/s\n",
                     bm->runtimeSec,
                     world ? world->boidCount : (size_t)cfg->boidCount,
                     cfg->threadCount,
                     bm->liveAvgMs,
                     overallAvgMs,
                     bm->liveTicksPerSecond);

    bm->intervalMsSum = 0.0;
    bm->intervalTickCount = 0;
    bm->lastUpdateUs = nowUs;
}

static bool run_live_game(const AppConfig* cfg, LiveBenchmarkState* bm) {
    World world;
    Input input;
    Renderer renderer;
    UpdateWinApi upd;
    Vec2 playerDir = {1.0f, 0.0f};
    const double dt = 1.0 / (double)cfg->stepsPerSecond;
    unsigned int seed = bm && bm->active ? 1234u : (unsigned int)(time_now_us() & 0xffffffffu);
    uint64_t loopStartUs = time_now_us();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    if (!setup_world_for_run(&world, cfg, seed)) {
        fprintf(stderr, "world_init failed\n");
        SDL_Quit();
        return false;
    }

    if (!renderer_init(&renderer, cfg->width, cfg->height)) {
        world_destroy(&world);
        SDL_Quit();
        return false;
    }

    if (!update_winapi_init(&upd, (size_t)cfg->threadCount)) {
        fprintf(stderr, "update_winapi_init failed\n");
        renderer_shutdown(&renderer);
        world_destroy(&world);
        SDL_Quit();
        return false;
    }

    input_init(&input);

    if (bm && bm->active) {
        bm->startUs = time_now_us();
        bm->lastUpdateUs = bm->startUs;
        bm->intervalMsSum = 0.0;
        bm->intervalTickCount = 0;
        bm->totalMsSum = 0.0;
        bm->totalTickCount = 0;
        bm->runtimeSec = 0.0;
        bm->liveAvgMs = 0.0;
        bm->liveTicksPerSecond = 0.0;

        benchmark_printf("interactive benchmark start run=winapi boids=%zu threads=%d\n",
                         world.boidCount,
                         cfg->threadCount);
    }

    while (true) {
        InputState st = input_poll(&input);
        Vec2 moveDir = {0.0f, 0.0f};
        double stepMs;

        if (st.quit) break;
        if (cfg->runtimeSeconds > 0) {
            double elapsedSec = (double)(time_now_us() - loopStartUs) / 1000000.0;
            if (elapsedSec >= (double)cfg->runtimeSeconds) break;
        }

        if (st.up) moveDir.y -= 1.0f;
        if (st.down) moveDir.y += 1.0f;
        if (st.left) moveDir.x -= 1.0f;
        if (st.right) moveDir.x += 1.0f;
        if (moveDir.x != 0.0f || moveDir.y != 0.0f) {
            playerDir = moveDir;
        }

        world_apply_player_input(&world, &st, dt);
        stepMs = world_step_winapi(&upd, &world, dt);

        live_benchmark_note_tick(bm, stepMs);
        live_benchmark_log_if_needed(cfg, &world, bm, false);
        renderer_draw_world(&renderer, &world, playerDir, stepMs, cfg->threadCount, bm);
        time_sleep_us((uint64_t)(dt * 1000000.0));
    }

    live_benchmark_log_if_needed(cfg, &world, bm, true);

    update_winapi_destroy(&upd);
    renderer_shutdown(&renderer);
    world_destroy(&world);
    SDL_Quit();
    return true;
}

enum {
    STARTUP_ID_WIDTH = 1001,
    STARTUP_ID_HEIGHT = 1002,
    STARTUP_ID_BOIDS = 1003,
    STARTUP_ID_THREADS = 1004,
    STARTUP_ID_STEPS = 1005,
    STARTUP_ID_START = 1006,
    STARTUP_ID_CANCEL = 1007,
};

typedef struct StartupPromptState {
    AppConfig* cfg;
    bool benchmarkPrompt;
    bool accepted;
    HWND boidsEdit;
    HWND threadsEdit;
    HWND stepsEdit;
} StartupPromptState;

static HWND startup_create_label(HWND parent, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
                           x, y, w, h, parent, NULL, GetModuleHandleW(NULL), NULL);
}

static HWND startup_create_edit(HWND parent, int id, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"EDIT", text,
                           WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
                           x, y, w, h, parent, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
}

static HWND startup_create_button(HWND parent, int id, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"BUTTON", text,
                           WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           x, y, w, h, parent, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
}

static int startup_read_edit_int(HWND edit, int fallbackValue) {
    wchar_t buffer[64];
    wchar_t* end = NULL;
    long value;

    if (!edit) return fallbackValue;
    GetWindowTextW(edit, buffer, (int)(sizeof(buffer) / sizeof(buffer[0])));
    if (buffer[0] == L'\0') return fallbackValue;

    value = wcstol(buffer, &end, 10);
    if (end == buffer) return fallbackValue;
    return (int)value;
}

static bool startup_collect_values(HWND hwnd, StartupPromptState* state) {
    AppConfig next = *state->cfg;

    next.boidCount = startup_read_edit_int(state->boidsEdit, next.boidCount);
    next.threadCount = startup_read_edit_int(state->threadsEdit, next.threadCount);
    next.benchmarkOnly = false;
    next.liveBenchmarkSession = false;

    if (state->benchmarkPrompt) {
        next.benchmarkSteps = startup_read_edit_int(state->stepsEdit, next.benchmarkSteps);
        next.liveBenchmarkSession = true;
    }

    if (next.boidCount <= 0 || next.threadCount <= 0) {
        MessageBoxW(hwnd, L"A boidok és a szálak száma legyen pozitív.",
                    L"Hibás adat", MB_ICONERROR | MB_OK);
        return false;
    }
    if (state->benchmarkPrompt && next.benchmarkSteps <= 0) {
        MessageBoxW(hwnd, L"Az összevetési lépések száma legyen pozitív.", L"Hibás adat", MB_ICONERROR | MB_OK);
        return false;
    }

    *state->cfg = next;
    state->accepted = true;
    DestroyWindow(hwnd);
    return true;
}

static LRESULT CALLBACK startup_prompt_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    StartupPromptState* state = (StartupPromptState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return TRUE;
    }
    case WM_CREATE: {
        wchar_t buffer[32];
        int y = 18;

        state = (StartupPromptState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        startup_create_label(hwnd,
                             state->benchmarkPrompt ? L"Add meg a mérési indítási adatokat." : L"Add meg a boids indítási adatait.",
                             18, y, 320, 18);
        y += 32;

        startup_create_label(hwnd, L"Boid darab:", 18, y + 3, 120, 20);
        swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%d", state->cfg->boidCount);
        state->boidsEdit = startup_create_edit(hwnd, STARTUP_ID_BOIDS, buffer, 150, y, 140, 24);
        y += 34;

        startup_create_label(hwnd, L"Szálak száma:", 18, y + 3, 120, 20);
        swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%d", state->cfg->threadCount);
        state->threadsEdit = startup_create_edit(hwnd, STARTUP_ID_THREADS, buffer, 150, y, 140, 24);
        y += 34;

        if (state->benchmarkPrompt) {
            startup_create_label(hwnd, L"Összevetési lépések:", 18, y + 3, 120, 20);
            swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%d",
                     state->cfg->benchmarkSteps > 0 ? state->cfg->benchmarkSteps : 500);
            state->stepsEdit = startup_create_edit(hwnd, STARTUP_ID_STEPS, buffer, 150, y, 140, 24);
            y += 34;
        }

        startup_create_button(hwnd, STARTUP_ID_START, L"Indítás", 70, y + 10, 90, 28);
        startup_create_button(hwnd, STARTUP_ID_CANCEL, L"Mégse", 180, y + 10, 90, 28);
        return 0;
    }
    case WM_COMMAND:
        if (!state) return 0;
        if (LOWORD(wParam) == STARTUP_ID_START) {
            startup_collect_values(hwnd, state);
            return 0;
        }
        if (LOWORD(wParam) == STARTUP_ID_CANCEL) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool prompt_startup_config(AppConfig* cfg, bool benchmarkPrompt) {
    static bool classRegistered = false;
    const wchar_t* className = L"BoidsWinApiStartupPromptWindow";
    StartupPromptState state;
    WNDCLASSW wc;
    RECT clientRect;
    const DWORD windowStyle = WS_CAPTION | WS_SYSMENU;
    const DWORD windowExStyle = WS_EX_DLGMODALFRAME;
    HWND hwnd;
    MSG msg;
    int width;
    int height;
    int x;
    int y;

    if (!cfg) return false;

    if (!classRegistered) {
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = startup_prompt_proc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = className;
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
        classRegistered = true;
    }

    memset(&state, 0, sizeof(state));
    state.cfg = cfg;
    state.benchmarkPrompt = benchmarkPrompt;

    clientRect.left = 0;
    clientRect.top = 0;
    clientRect.right = 340;
    clientRect.bottom = benchmarkPrompt ? 210 : 180;
    AdjustWindowRectEx(&clientRect, windowStyle, FALSE, windowExStyle);
    width = clientRect.right - clientRect.left;
    height = clientRect.bottom - clientRect.top;
    x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    hwnd = CreateWindowExW(windowExStyle,
                           className,
                           benchmarkPrompt ? L"WinAPI boids mérés indítás" : L"WinAPI boids indítás",
                           windowStyle | WS_VISIBLE,
                           x, y, width, height,
                           NULL, NULL, GetModuleHandleW(NULL), &state);
    if (!hwnd) return false;

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return state.accepted;
}

int main(int argc, char** argv) {
    const bool benchmarkExe = exe_name_is_benchmark(argc > 0 ? argv[0] : NULL);
    AppConfig cfg = {
        .width = 80,
        .height = 25,
        .boidCount = 200,
        .threadCount = 4,
        .stepsPerSecond = 30,
        .benchmarkOnly = false,
        .liveBenchmarkSession = false,
        .benchmarkSteps = 0,
        .runtimeSeconds = 0,
    };

    if (argc <= 1) {
        cfg.benchmarkSteps = 500;
        if (!prompt_startup_config(&cfg, benchmarkExe)) {
            return 0;
        }
    } else {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--help") == 0) {
                usage(argv[0]);
                return 0;
            }
            if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) cfg.threadCount = parse_int(argv[++i], cfg.threadCount);
            else if (strcmp(argv[i], "--boids") == 0 && i + 1 < argc) cfg.boidCount = parse_int(argv[++i], cfg.boidCount);
            else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) cfg.width = parse_int(argv[++i], cfg.width);
            else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) cfg.height = parse_int(argv[++i], cfg.height);
            else if (strcmp(argv[i], "--benchmark") == 0 && i + 1 < argc) {
                cfg.benchmarkOnly = true;
                cfg.benchmarkSteps = parse_int(argv[++i], cfg.benchmarkSteps);
            } else if (strcmp(argv[i], "--live-benchmark") == 0 && i + 1 < argc) {
                cfg.liveBenchmarkSession = true;
                cfg.benchmarkSteps = parse_int(argv[++i], cfg.benchmarkSteps);
            } else if (strcmp(argv[i], "--compare") == 0) {
                cfg.benchmarkOnly = true;
            } else if (strcmp(argv[i], "--runtime") == 0 && i + 1 < argc) {
                cfg.runtimeSeconds = parse_int(argv[++i], cfg.runtimeSeconds);
            } else {
                fprintf(stderr, "Unknown arg: %s\n", argv[i]);
                usage(argv[0]);
                return 2;
            }
        }
    }

    if (cfg.width <= 5 || cfg.height <= 5 || cfg.boidCount <= 0 || cfg.threadCount <= 0) {
        fprintf(stderr, "Invalid config. Use --help\n");
        return 2;
    }
    if ((cfg.benchmarkOnly || cfg.liveBenchmarkSession) && cfg.benchmarkSteps <= 0) {
        fprintf(stderr, "Benchmark mode needs a positive tick count. Use --benchmark N or --live-benchmark N\n");
        return 2;
    }
    if (cfg.runtimeSeconds < 0) {
        fprintf(stderr, "Runtime must be zero or positive. Use --runtime S\n");
        return 2;
    }

    if (cfg.benchmarkOnly || cfg.liveBenchmarkSession) {
        benchmark_attach_console();
        (void)benchmark_open_log_file();
    }

    if (cfg.benchmarkOnly) {
        bool ok = run_compare_benchmark(&cfg, NULL);
        benchmark_close_log_file();
        return ok ? 0 : 1;
    }

    if (cfg.liveBenchmarkSession) {
        LiveBenchmarkState bm;
        bool ok = run_compare_benchmark(&cfg, &bm) && run_live_game(&cfg, &bm);
        double overallAvgMs = 0.0;

        if (bm.totalTickCount > 0) {
            overallAvgMs = bm.totalMsSum / (double)bm.totalTickCount;
        }

        benchmark_printf("interactive benchmark end runtime=%.1fs avg=%.3f ms/tick ticks=%.2f/s log=%s\n",
                         bm.runtimeSec,
                         overallAvgMs,
                         bm.liveTicksPerSecond,
                         benchmark_log_path() ? benchmark_log_path() : "-");
        benchmark_close_log_file();
        return ok ? 0 : 1;
    }

    return run_live_game(&cfg, NULL) ? 0 : 1;
}