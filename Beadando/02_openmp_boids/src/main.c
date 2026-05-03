#include "boids.h"

#include <math.h>
#include <stdarg.h>
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
#else
#include <time.h>
#endif

typedef struct AppConfig {
    int width;
    int height;
    int boidCount;
    int threads;
    int stepsPerSecond;
    bool benchmarkOnly;
    bool liveBenchmarkSession;
    int benchmarkSteps;
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
    double ompAvgMs;
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
#ifdef _WIN32
    static LARGE_INTEGER freq;
    static int init = 0;
    LARGE_INTEGER now;

    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = 1;
    }

    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart * 1000000ULL) / (uint64_t)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000ULL);
#endif
}

static void time_sleep_us(uint64_t us) {
    Uint32 ms;

    if (us == 0) return;
    ms = (Uint32)(us / 1000ULL);
    if (ms == 0) ms = 1;
    SDL_Delay(ms);
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

#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(g_benchmarkLogPath, sizeof(g_benchmarkLogPath),
             "boids_openmp_benchmark_%04u%02u%02u_%02u%02u%02u.txt",
             (unsigned)st.wYear,
             (unsigned)st.wMonth,
             (unsigned)st.wDay,
             (unsigned)st.wHour,
             (unsigned)st.wMinute,
             (unsigned)st.wSecond);
#else
    snprintf(g_benchmarkLogPath, sizeof(g_benchmarkLogPath), "boids_openmp_benchmark_last.txt");
#endif

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
#ifdef _WIN32
    static bool attached = false;

    if (attached) return;
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        (void)freopen("CONOUT$", "w", stdout);
        (void)freopen("CONOUT$", "w", stderr);
        (void)freopen("CONIN$", "r", stdin);
        attached = true;
    }
#endif
}

static void print_usage(const char* exe) {
    printf("Usage: %s [--width W] [--height H] [--boids N] [--threads N]\n", exe);
    printf("       %s --benchmark N [--compare] [--width W] [--height H] [--boids N] [--threads N]\n", exe);
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

static void set_group_color(SDL_Renderer* renderer, const Boid* boid) {
    static const unsigned char palette[][3] = {
        {120, 200, 255},
        {255, 160, 120},
        {170, 230, 120},
        {255, 220, 120},
        {210, 170, 255},
        {120, 240, 210},
        {255, 130, 190},
        {190, 190, 255},
    };
    const size_t paletteCount = sizeof(palette) / sizeof(palette[0]);
    size_t index = (size_t)(boid->group % (unsigned char)paletteCount);

    if (boid->predator) {
        SDL_SetRenderDrawColor(renderer, 240, 90, 90, 255);
        return;
    }

    SDL_SetRenderDrawColor(renderer,
                           palette[index][0],
                           palette[index][1],
                           palette[index][2],
                           255);
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

static void renderer_update_title(Renderer* renderer,
                                  const World* world,
                                  int threads,
                                  double stepMs,
                                  const LiveBenchmarkState* bm) {
    char title[512];

    if (bm && bm->active) {
        snprintf(title, sizeof(title),
                 "Boids | OpenMP threads=%d | tick=%.3f ms | seq=%.3f | omp=%.3f | speedup=%.2fx | live=%.3f ms/tick",
                 threads,
                 stepMs,
                 bm->seqAvgMs,
                 bm->ompAvgMs,
                 bm->speedup,
                 bm->liveAvgMs);
    } else {
        snprintf(title, sizeof(title),
                 "Boids | OpenMP boids=%zu | threads=%d | tick=%.3f ms | WASD move | Q quit",
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

    renderer->window = SDL_CreateWindow("Boids | OpenMP",
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
        float px;
        float py;

        if (!boid->alive) continue;

        set_group_color(renderer->renderer, boid);
        px = (float)offsetX + boid->pos.x * (float)cellSize;
        py = (float)offsetY + boid->pos.y * (float)cellSize;
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

static BenchmarkResult run_benchmark_ticks(const AppConfig* cfg, bool parallel) {
    BenchmarkResult result = {0};
    World world;
    const double dt = 1.0 / (double)cfg->stepsPerSecond;

    if (!setup_world_for_run(&world, cfg, 1234u)) {
        return result;
    }

    for (int i = 0; i < BENCHMARK_WARMUP_TICKS; i++) {
        if (parallel) {
            (void)world_step_openmp(&world, cfg->threads, dt);
        } else {
            (void)world_step_seq(&world, dt);
        }
    }

    for (int i = 0; i < cfg->benchmarkSteps; i++) {
        result.totalMs += parallel ? world_step_openmp(&world, cfg->threads, dt)
                                   : world_step_seq(&world, dt);
    }

    if (cfg->benchmarkSteps > 0) {
        result.avgMs = result.totalMs / (double)cfg->benchmarkSteps;
    }
    if (result.totalMs > 0.0) {
        result.ticksPerSecond = (double)cfg->benchmarkSteps * 1000.0 / result.totalMs;
    }

    world_destroy(&world);
    return result;
}

static bool run_compare_benchmark(const AppConfig* cfg, LiveBenchmarkState* bm) {
    BenchmarkResult seqResult;
    BenchmarkResult ompResult;

    if (cfg->benchmarkSteps <= 0) return false;

    seqResult = run_benchmark_ticks(cfg, false);
    ompResult = run_benchmark_ticks(cfg, true);

    if (seqResult.avgMs <= 0.0 || ompResult.avgMs <= 0.0) {
        return false;
    }

    if (bm) {
        memset(bm, 0, sizeof(*bm));
        bm->active = true;
        bm->seqAvgMs = seqResult.avgMs;
        bm->ompAvgMs = ompResult.avgMs;
        bm->speedup = seqResult.avgMs / ompResult.avgMs;
    }

    benchmark_printf("benchmark compare section=boids_step width=%d height=%d boids=%d ticks=%d\n",
                     cfg->width,
                     cfg->height,
                     cfg->boidCount,
                     cfg->benchmarkSteps);
    benchmark_printf("  seq:        avg=%.3f ms/tick | ticks=%.2f/s\n", seqResult.avgMs, seqResult.ticksPerSecond);
    benchmark_printf("  openmp(%d): avg=%.3f ms/tick | ticks=%.2f/s\n", cfg->threads, ompResult.avgMs, ompResult.ticksPerSecond);
    benchmark_printf("  speedup:    %.2fx\n", seqResult.avgMs / ompResult.avgMs);
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

    benchmark_printf("[live %.1fs] run=openmp boids=%zu threads=%d interval=%.3f ms/tick overall=%.3f ms/tick ticks=%.2f/s\n",
                     bm->runtimeSec,
                     world ? world->boidCount : (size_t)cfg->boidCount,
                     cfg->threads,
                     bm->liveAvgMs,
                     overallAvgMs,
                     bm->liveTicksPerSecond);

    bm->intervalMsSum = 0.0;
    bm->intervalTickCount = 0;
    bm->lastUpdateUs = nowUs;
}

static bool run_live_game(const AppConfig* cfg, LiveBenchmarkState* bm) {
    World world;
    Renderer renderer;
    Input input;
    Vec2 playerDir = {1.0f, 0.0f};
    const double dt = 1.0 / (double)cfg->stepsPerSecond;
    unsigned int seed = bm && bm->active ? 1234u : (unsigned int)(time_now_us() & 0xffffffffu);

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

        benchmark_printf("interactive benchmark start run=openmp boids=%zu threads=%d\n",
                         world.boidCount,
                         cfg->threads);
    }

    while (true) {
        InputState st = input_poll(&input);
        double stepMs;
        Vec2 moveDir = {0.0f, 0.0f};

        if (st.quit) break;

        if (st.up) moveDir.y -= 1.0f;
        if (st.down) moveDir.y += 1.0f;
        if (st.left) moveDir.x -= 1.0f;
        if (st.right) moveDir.x += 1.0f;
        if (moveDir.x != 0.0f || moveDir.y != 0.0f) {
            playerDir = moveDir;
        }

        world_apply_player_input(&world, &st, dt);
        stepMs = world_step_openmp(&world, cfg->threads, dt);

        live_benchmark_note_tick(bm, stepMs);
        live_benchmark_log_if_needed(cfg, &world, bm, false);
        renderer_draw_world(&renderer, &world, playerDir, stepMs, cfg->threads, bm);
        time_sleep_us((uint64_t)(dt * 1000000.0));
    }

    live_benchmark_log_if_needed(cfg, &world, bm, true);

    renderer_shutdown(&renderer);
    world_destroy(&world);
    SDL_Quit();
    return true;
}

#ifdef _WIN32
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
    HWND widthEdit;
    HWND heightEdit;
    HWND boidsEdit;
    HWND threadsEdit;
    HWND stepsEdit;
} StartupPromptState;

static HWND startup_create_label(HWND parent, const char* text, int x, int y, int w, int h) {
    return CreateWindowExA(0, "STATIC", text, WS_CHILD | WS_VISIBLE,
                           x, y, w, h, parent, NULL, GetModuleHandleA(NULL), NULL);
}

static HWND startup_create_edit(HWND parent, int id, const char* text, int x, int y, int w, int h) {
    return CreateWindowExA(0, "EDIT", text,
                           WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
                           x, y, w, h, parent, (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);
}

static HWND startup_create_button(HWND parent, int id, const char* text, int x, int y, int w, int h) {
    return CreateWindowExA(0, "BUTTON", text,
                           WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           x, y, w, h, parent, (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);
}

static int startup_read_edit_int(HWND edit, int fallbackValue) {
    char buffer[64];
    if (!edit) return fallbackValue;
    GetWindowTextA(edit, buffer, (int)sizeof(buffer));
    return parse_int(buffer, fallbackValue);
}

static bool startup_collect_values(HWND hwnd, StartupPromptState* state) {
    AppConfig next = *state->cfg;

    next.width = startup_read_edit_int(state->widthEdit, next.width);
    next.height = startup_read_edit_int(state->heightEdit, next.height);
    next.boidCount = startup_read_edit_int(state->boidsEdit, next.boidCount);
    next.threads = startup_read_edit_int(state->threadsEdit, next.threads);
    next.benchmarkOnly = false;
    next.liveBenchmarkSession = false;

    if (state->benchmarkPrompt) {
        next.benchmarkSteps = startup_read_edit_int(state->stepsEdit, next.benchmarkSteps);
        next.liveBenchmarkSession = true;
    }

    if (next.width <= 5 || next.height <= 5 || next.boidCount <= 0 || next.threads <= 0) {
        MessageBoxA(hwnd, "A meretek legyenek ervenyesek, a boid es a szal szam legyen pozitiv.",
                    "Hibas adat", MB_ICONERROR | MB_OK);
        return false;
    }
    if (state->benchmarkPrompt && next.benchmarkSteps <= 0) {
        MessageBoxA(hwnd, "Az osszevetesi tickek szama legyen pozitiv.", "Hibas adat", MB_ICONERROR | MB_OK);
        return false;
    }

    *state->cfg = next;
    state->accepted = true;
    DestroyWindow(hwnd);
    return true;
}

static LRESULT CALLBACK startup_prompt_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    StartupPromptState* state = (StartupPromptState*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return TRUE;
    }
    case WM_CREATE: {
        char buffer[32];
        int y = 18;

        state = (StartupPromptState*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
        startup_create_label(hwnd,
                             state->benchmarkPrompt ? "Add meg a meresi inditasi adatokat." : "Add meg a boids inditasi adatait.",
                             18, y, 340, 18);
        y += 32;

        startup_create_label(hwnd, "Szelesseg:", 18, y + 3, 120, 20);
        snprintf(buffer, sizeof(buffer), "%d", state->cfg->width);
        state->widthEdit = startup_create_edit(hwnd, STARTUP_ID_WIDTH, buffer, 170, y, 140, 24);
        y += 34;

        startup_create_label(hwnd, "Magassag:", 18, y + 3, 120, 20);
        snprintf(buffer, sizeof(buffer), "%d", state->cfg->height);
        state->heightEdit = startup_create_edit(hwnd, STARTUP_ID_HEIGHT, buffer, 170, y, 140, 24);
        y += 34;

        startup_create_label(hwnd, "Boid darab:", 18, y + 3, 120, 20);
        snprintf(buffer, sizeof(buffer), "%d", state->cfg->boidCount);
        state->boidsEdit = startup_create_edit(hwnd, STARTUP_ID_BOIDS, buffer, 170, y, 140, 24);
        y += 34;

        startup_create_label(hwnd, "Szalak szama:", 18, y + 3, 120, 20);
        snprintf(buffer, sizeof(buffer), "%d", state->cfg->threads);
        state->threadsEdit = startup_create_edit(hwnd, STARTUP_ID_THREADS, buffer, 170, y, 140, 24);
        y += 34;

        if (state->benchmarkPrompt) {
            startup_create_label(hwnd, "Osszevetesi tickek:", 18, y + 3, 120, 20);
            snprintf(buffer, sizeof(buffer), "%d", state->cfg->benchmarkSteps > 0 ? state->cfg->benchmarkSteps : 500);
            state->stepsEdit = startup_create_edit(hwnd, STARTUP_ID_STEPS, buffer, 170, y, 140, 24);
            y += 34;
        }

        startup_create_button(hwnd, STARTUP_ID_START, "Inditas", 82, y + 10, 90, 28);
        startup_create_button(hwnd, STARTUP_ID_CANCEL, "Megse", 192, y + 10, 90, 28);
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

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static bool prompt_startup_config(AppConfig* cfg, bool benchmarkPrompt) {
    static bool classRegistered = false;
    const char* className = "BoidsOpenMpStartupPromptWindow";
    StartupPromptState state;
    WNDCLASSA wc;
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
        wc.hInstance = GetModuleHandleA(NULL);
        wc.lpszClassName = className;
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
        classRegistered = true;
    }

    memset(&state, 0, sizeof(state));
    state.cfg = cfg;
    state.benchmarkPrompt = benchmarkPrompt;

    clientRect.left = 0;
    clientRect.top = 0;
    clientRect.right = 360;
    clientRect.bottom = benchmarkPrompt ? 286 : 252;
    AdjustWindowRectEx(&clientRect, windowStyle, FALSE, windowExStyle);
    width = clientRect.right - clientRect.left;
    height = clientRect.bottom - clientRect.top;
    x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    hwnd = CreateWindowExA(windowExStyle,
                           className,
                           benchmarkPrompt ? "OpenMP boids meres inditas" : "OpenMP boids inditas",
                           windowStyle | WS_VISIBLE,
                           x, y, width, height,
                           NULL, NULL, GetModuleHandleA(NULL), &state);
    if (!hwnd) return false;

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return state.accepted;
}
#else
static bool prompt_startup_config(AppConfig* cfg, bool benchmarkPrompt) {
    (void)cfg;
    (void)benchmarkPrompt;
    return false;
}
#endif

int main(int argc, char** argv) {
    const bool benchmarkExe = exe_name_is_benchmark(argc > 0 ? argv[0] : NULL);
    AppConfig cfg = {
        .width = 80,
        .height = 25,
        .boidCount = 200,
        .threads = 4,
        .stepsPerSecond = 30,
        .benchmarkOnly = false,
        .liveBenchmarkSession = false,
        .benchmarkSteps = 0,
    };

    if (argc <= 1) {
        cfg.benchmarkSteps = 500;
        if (!prompt_startup_config(&cfg, benchmarkExe)) {
            return 0;
        }
    } else {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) cfg.width = parse_int(argv[++i], cfg.width);
            else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) cfg.height = parse_int(argv[++i], cfg.height);
            else if (strcmp(argv[i], "--boids") == 0 && i + 1 < argc) cfg.boidCount = parse_int(argv[++i], cfg.boidCount);
            else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) cfg.threads = parse_int(argv[++i], cfg.threads);
            else if (strcmp(argv[i], "--benchmark") == 0 && i + 1 < argc) {
                cfg.benchmarkOnly = true;
                cfg.benchmarkSteps = parse_int(argv[++i], cfg.benchmarkSteps);
            } else if (strcmp(argv[i], "--compare") == 0) {
                cfg.benchmarkOnly = true;
            } else if (strcmp(argv[i], "--help") == 0) {
                print_usage(argv[0]);
                return 0;
            } else {
                fprintf(stderr, "Unknown arg: %s\n", argv[i]);
                print_usage(argv[0]);
                return 2;
            }
        }
    }

    if (cfg.width <= 5 || cfg.height <= 5 || cfg.boidCount <= 0 || cfg.threads <= 0) {
        fprintf(stderr, "Invalid config. Use --help\n");
        return 2;
    }
    if (cfg.benchmarkOnly && cfg.benchmarkSteps <= 0) {
        fprintf(stderr, "Benchmark mode needs a positive tick count. Use --benchmark N\n");
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