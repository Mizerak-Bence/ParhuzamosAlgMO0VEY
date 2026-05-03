#include "life.h"

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

typedef struct SDL_Rect {
    int x;
    int y;
    int w;
    int h;
} SDL_Rect;

typedef struct SDL_Keysym {
    SDL_Keycode sym;
} SDL_Keysym;

typedef struct SDL_KeyboardEvent {
    SDL_Keysym keysym;
} SDL_KeyboardEvent;

typedef struct SDL_WindowEvent {
    Uint8 event;
} SDL_WindowEvent;

typedef struct SDL_Event {
    Uint32 type;
    SDL_KeyboardEvent key;
    SDL_WindowEvent window;
} SDL_Event;

#define SDL_INIT_VIDEO 0x00000020u
#define SDL_QUIT 0x00000100u
#define SDL_KEYDOWN 0x00000300u
#define SDL_WINDOWEVENT 0x00000200u
#define SDL_WINDOWEVENT_SIZE_CHANGED 0x05u

#define SDLK_ESCAPE 27
#define SDLK_w 'w'
#define SDLK_a 'a'
#define SDLK_s 's'
#define SDLK_d 'd'
#define SDLK_q 'q'
#define SDLK_p 'p'
#define SDLK_SPACE ' '

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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

typedef struct Input {
    int dummy;
} Input;

typedef struct InputState {
    bool up;
    bool down;
    bool left;
    bool right;
    bool toggle;
    bool pauseToggle;
    bool quit;
} InputState;

typedef struct Renderer {
    SDL_Window* window;
    SDL_Renderer* renderer;
    int winW;
    int winH;
} Renderer;

typedef struct BenchmarkResult {
    double totalMs;
    double avgMs;
    double stepsPerSecond;
} BenchmarkResult;

typedef struct LiveBenchmarkState {
    bool active;
    double seqAvgMs;
    double ompAvgMs;
    double speedup;
    double runtimeSec;
    double liveAvgMs;
    double liveStepsPerSecond;
    uint64_t startUs;
    uint64_t lastUpdateUs;
    double intervalMsSum;
    int intervalStepCount;
} LiveBenchmarkState;

enum {
    BENCHMARK_WARMUP_STEPS = 100,
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
    return 0;
#endif
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
             "life_openmp_benchmark_%04u%02u%02u_%02u%02u%02u.txt",
             (unsigned)st.wYear,
             (unsigned)st.wMonth,
             (unsigned)st.wDay,
             (unsigned)st.wHour,
             (unsigned)st.wMinute,
             (unsigned)st.wSecond);
#else
    snprintf(g_benchmarkLogPath, sizeof(g_benchmarkLogPath), "life_openmp_benchmark_last.txt");
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
    printf("Usage: %s [--width W] [--height H] [--threads N]\n", exe);
    printf("       %s --benchmark N [--compare] [--width W] [--height H] [--threads N]\n", exe);
}

static void input_init(Input* input) {
    (void)input;
}

static InputState input_poll(Input* input) {
    InputState st = {0};
    SDL_Event e;

    (void)input;

    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            st.quit = true;
        } else if (e.type == SDL_KEYDOWN) {
            SDL_Keycode key = e.key.keysym.sym;
            if (key == SDLK_ESCAPE || key == SDLK_q) st.quit = true;
            if (key == SDLK_w) st.up = true;
            if (key == SDLK_s) st.down = true;
            if (key == SDLK_a) st.left = true;
            if (key == SDLK_d) st.right = true;
            if (key == SDLK_SPACE || key == ' ') st.toggle = true;
            if (key == SDLK_p || key == 'p' || key == 'P') st.pauseToggle = true;
        }
    }

    return st;
}

static int life_idx(const Life* life, int x, int y) {
    return y * life->width + x;
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

static void renderer_update_title(Renderer* r,
                                  bool running,
                                  int threads,
                                  double stepMs,
                                  const LiveBenchmarkState* bm) {
    char title[512];

    if (bm && bm->active) {
        snprintf(title, sizeof(title),
                 "Game of Life | OpenMP threads=%d | %s | step=%.3f ms | seq=%.3f | omp=%.3f | speedup=%.2fx | live=%.3f ms/step",
                 threads,
                 running ? "RUN" : "PAUSE",
                 stepMs,
                 bm->seqAvgMs,
                 bm->ompAvgMs,
                 bm->speedup,
                 bm->liveAvgMs);
    } else {
        snprintf(title, sizeof(title),
                 "Game of Life | OpenMP threads=%d | %s | step=%.3f ms | WASD move | SPACE toggle | P pause | Q quit",
                 threads,
                 running ? "RUN" : "PAUSE",
                 stepMs);
    }

    SDL_SetWindowTitle(r->window, title);
}

static bool renderer_init(Renderer* r, int width, int height) {
    int winW = width * 12;
    int winH = height * 12;

    memset(r, 0, sizeof(*r));

    if (winW < 720) winW = 720;
    if (winH < 520) winH = 520;

    r->window = SDL_CreateWindow("Game of Life | OpenMP",
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

static void renderer_draw(Renderer* r,
                          const Life* life,
                          int cursorX,
                          int cursorY,
                          bool running,
                          int threads,
                          double stepMs,
                          const LiveBenchmarkState* bm) {
    int cellSize;
    int boardW;
    int boardH;
    int offsetX;
    int offsetY;

    renderer_update_metrics(r);
    renderer_update_title(r, running, threads, stepMs, bm);

    cellSize = r->winW / life->width;
    if (r->winH / life->height < cellSize) cellSize = r->winH / life->height;
    if (cellSize < 2) cellSize = 2;

    boardW = cellSize * life->width;
    boardH = cellSize * life->height;
    offsetX = (r->winW - boardW) / 2;
    offsetY = (r->winH - boardH) / 2;

    SDL_SetRenderDrawColor(r->renderer, 18, 20, 24, 255);
    SDL_RenderClear(r->renderer);

    SDL_SetRenderDrawColor(r->renderer, 28, 32, 40, 255);
    draw_rect_filled(r->renderer, offsetX, offsetY, boardW, boardH);

    for (int y = 0; y < life->height; y++) {
        for (int x = 0; x < life->width; x++) {
            if (!life->a[life_idx(life, x, y)]) continue;

            SDL_SetRenderDrawColor(r->renderer, 160, 220, 120, 255);
            draw_rect_filled(r->renderer,
                             offsetX + x * cellSize + 1,
                             offsetY + y * cellSize + 1,
                             cellSize - 2,
                             cellSize - 2);
        }
    }

    SDL_SetRenderDrawColor(r->renderer, running ? 255 : 220, running ? 186 : 90, 70, 255);
    draw_rect_outline(r->renderer,
                      offsetX + cursorX * cellSize,
                      offsetY + cursorY * cellSize,
                      cellSize,
                      cellSize);

    SDL_SetRenderDrawColor(r->renderer, 90, 96, 110, 255);
    draw_rect_outline(r->renderer, offsetX, offsetY, boardW, boardH);
    SDL_RenderPresent(r->renderer);
}

static bool setup_life_for_run(Life* life, const AppConfig* cfg) {
    int cursorX;
    int cursorY;

    if (!life_init(life, cfg->width, cfg->height)) {
        return false;
    }

    cursorX = cfg->width / 2;
    cursorY = cfg->height / 2;
    life_seed_glider(life, cursorX, cursorY);
    return true;
}

static BenchmarkResult run_benchmark_steps(const AppConfig* cfg, bool parallel) {
    BenchmarkResult result = {0};
    Life life;

    if (!setup_life_for_run(&life, cfg)) {
        return result;
    }

    for (int i = 0; i < BENCHMARK_WARMUP_STEPS; i++) {
        if (parallel) {
            (void)life_step_openmp(&life, cfg->threads);
        } else {
            (void)life_step_seq(&life);
        }
    }

    for (int i = 0; i < cfg->benchmarkSteps; i++) {
        result.totalMs += parallel ? life_step_openmp(&life, cfg->threads) : life_step_seq(&life);
    }

    if (cfg->benchmarkSteps > 0) {
        result.avgMs = result.totalMs / (double)cfg->benchmarkSteps;
    }
    if (result.totalMs > 0.0) {
        result.stepsPerSecond = (double)cfg->benchmarkSteps * 1000.0 / result.totalMs;
    }

    life_destroy(&life);
    return result;
}

static bool run_compare_benchmark(const AppConfig* cfg, LiveBenchmarkState* bm) {
    BenchmarkResult seqResult;
    BenchmarkResult ompResult;

    if (cfg->benchmarkSteps <= 0) return false;

    seqResult = run_benchmark_steps(cfg, false);
    ompResult = run_benchmark_steps(cfg, true);

    if (seqResult.avgMs <= 0.0 || ompResult.avgMs <= 0.0) {
        return false;
    }

    if (bm) {
        memset(bm, 0, sizeof(*bm));
        bm->active = true;
        bm->seqAvgMs = seqResult.avgMs;
        bm->ompAvgMs = ompResult.avgMs;
        bm->speedup = seqResult.avgMs / ompResult.avgMs;
        bm->startUs = time_now_us();
        bm->lastUpdateUs = bm->startUs;
    }

    benchmark_printf("benchmark compare section=life_step width=%d height=%d steps=%d\n",
                     cfg->width,
                     cfg->height,
                     cfg->benchmarkSteps);
    benchmark_printf("  seq:      avg=%.3f ms/step | steps=%.2f/s\n", seqResult.avgMs, seqResult.stepsPerSecond);
    benchmark_printf("  openmp(%d): avg=%.3f ms/step | steps=%.2f/s\n", cfg->threads, ompResult.avgMs, ompResult.stepsPerSecond);
    benchmark_printf("  speedup:  %.2fx\n", seqResult.avgMs / ompResult.avgMs);
    return true;
}

static void live_benchmark_note_step(LiveBenchmarkState* bm, double stepMs) {
    if (!bm || !bm->active) return;
    bm->intervalMsSum += stepMs;
    bm->intervalStepCount++;
}

static void live_benchmark_update(LiveBenchmarkState* bm) {
    uint64_t nowUs;
    double intervalSec;

    if (!bm || !bm->active) return;

    nowUs = time_now_us();
    bm->runtimeSec = (double)(nowUs - bm->startUs) / 1000000.0;

    if (nowUs - bm->lastUpdateUs < 1000000ULL) return;

    intervalSec = (double)(nowUs - bm->lastUpdateUs) / 1000000.0;
    if (bm->intervalStepCount > 0) {
        bm->liveAvgMs = bm->intervalMsSum / (double)bm->intervalStepCount;
        bm->liveStepsPerSecond = (double)bm->intervalStepCount / intervalSec;
    }

    bm->intervalMsSum = 0.0;
    bm->intervalStepCount = 0;
    bm->lastUpdateUs = nowUs;
}

static bool run_live_game(const AppConfig* cfg, LiveBenchmarkState* bm) {
    Life life;
    Renderer r;
    Input in;
    int cursorX = cfg->width / 2;
    int cursorY = cfg->height / 2;
    bool running = true;

    if (!setup_life_for_run(&life, cfg)) {
        fprintf(stderr, "life_init failed\n");
        return false;
    }

    renderer_init(&r, cfg->width, cfg->height);
    input_init(&in);

    while (true) {
        InputState st = input_poll(&in);
        double stepMs = 0.0;

        if (st.quit) break;
        if (st.pauseToggle) running = !running;
        if (st.left && cursorX > 0) cursorX--;
        if (st.right && cursorX < cfg->width - 1) cursorX++;
        if (st.up && cursorY > 0) cursorY--;
        if (st.down && cursorY < cfg->height - 1) cursorY++;
        if (st.toggle) life_toggle(&life, cursorX, cursorY);

        if (running) {
            stepMs = life_step_openmp(&life, cfg->threads);
            live_benchmark_note_step(bm, stepMs);
        }

        live_benchmark_update(bm);
        renderer_draw(&r, &life, cursorX, cursorY, running, cfg->threads, stepMs, bm);
    }

    renderer_shutdown(&r);
    life_destroy(&life);
    return true;
}

#ifdef _WIN32
enum {
    STARTUP_ID_WIDTH = 1001,
    STARTUP_ID_HEIGHT = 1002,
    STARTUP_ID_THREADS = 1003,
    STARTUP_ID_STEPS = 1004,
    STARTUP_ID_START = 1005,
    STARTUP_ID_CANCEL = 1006,
};

typedef struct StartupPromptState {
    AppConfig* cfg;
    bool benchmarkPrompt;
    bool accepted;
    HWND widthEdit;
    HWND heightEdit;
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
    next.threads = startup_read_edit_int(state->threadsEdit, next.threads);
    next.benchmarkOnly = false;
    next.liveBenchmarkSession = false;

    if (state->benchmarkPrompt) {
        next.benchmarkSteps = startup_read_edit_int(state->stepsEdit, next.benchmarkSteps);
        next.liveBenchmarkSession = true;
    }

    if (next.width <= 5 || next.height <= 5 || next.threads <= 0) {
        MessageBoxA(hwnd, "A meretek legyenek ervenyesek, a szalak szama legyen pozitiv.", "Hibas adat", MB_ICONERROR | MB_OK);
        return false;
    }
    if (state->benchmarkPrompt && next.benchmarkSteps <= 0) {
        MessageBoxA(hwnd, "Az osszevetesi lepesek szama legyen pozitiv.", "Hibas adat", MB_ICONERROR | MB_OK);
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
                             state->benchmarkPrompt ? "Add meg a meresi inditasi adatokat." : "Add meg a jatek inditasi adatait.",
                             18, y, 320, 18);
        y += 32;

        startup_create_label(hwnd, "Szelesseg:", 18, y + 3, 120, 20);
        snprintf(buffer, sizeof(buffer), "%d", state->cfg->width);
        state->widthEdit = startup_create_edit(hwnd, STARTUP_ID_WIDTH, buffer, 150, y, 140, 24);
        y += 34;

        startup_create_label(hwnd, "Magassag:", 18, y + 3, 120, 20);
        snprintf(buffer, sizeof(buffer), "%d", state->cfg->height);
        state->heightEdit = startup_create_edit(hwnd, STARTUP_ID_HEIGHT, buffer, 150, y, 140, 24);
        y += 34;

        startup_create_label(hwnd, "Szalak szama:", 18, y + 3, 120, 20);
        snprintf(buffer, sizeof(buffer), "%d", state->cfg->threads);
        state->threadsEdit = startup_create_edit(hwnd, STARTUP_ID_THREADS, buffer, 150, y, 140, 24);
        y += 34;

        if (state->benchmarkPrompt) {
            startup_create_label(hwnd, "Osszevetesi lepesek:", 18, y + 3, 120, 20);
            snprintf(buffer, sizeof(buffer), "%d", state->cfg->benchmarkSteps > 0 ? state->cfg->benchmarkSteps : 500);
            state->stepsEdit = startup_create_edit(hwnd, STARTUP_ID_STEPS, buffer, 150, y, 140, 24);
            y += 34;
        }

        startup_create_button(hwnd, STARTUP_ID_START, "Inditas", 70, y + 10, 90, 28);
        startup_create_button(hwnd, STARTUP_ID_CANCEL, "Megse", 180, y + 10, 90, 28);
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
    const char* className = "LifeOpenMpStartupPromptWindow";
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
    clientRect.right = 340;
    clientRect.bottom = benchmarkPrompt ? 220 : 190;
    AdjustWindowRectEx(&clientRect, windowStyle, FALSE, windowExStyle);
    width = clientRect.right - clientRect.left;
    height = clientRect.bottom - clientRect.top;
    x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    hwnd = CreateWindowExA(windowExStyle,
                           className,
                           benchmarkPrompt ? "OpenMP meres inditas" : "OpenMP jatek inditas",
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
        .threads = 4,
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

    if (cfg.width <= 5 || cfg.height <= 5 || cfg.threads <= 0) {
        fprintf(stderr, "Invalid config. Use --help\n");
        return 2;
    }
    if (cfg.benchmarkOnly && cfg.benchmarkSteps <= 0) {
        fprintf(stderr, "Benchmark mode needs a positive step count. Use --benchmark N\n");
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

        benchmark_printf("interactive benchmark end runtime=%.1fs avg=%.3f ms/step steps=%.2f/s log=%s\n",
                         bm.runtimeSec,
                         bm.liveAvgMs,
                         bm.liveStepsPerSecond,
                         benchmark_log_path() ? benchmark_log_path() : "-");
        benchmark_close_log_file();
        return ok ? 0 : 1;
    }

    return run_live_game(&cfg, NULL) ? 0 : 1;
}
