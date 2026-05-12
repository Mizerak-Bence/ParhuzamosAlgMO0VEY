#include "boids.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
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
typedef struct SDL_Surface SDL_Surface;
typedef struct SDL_Texture SDL_Texture;

typedef struct SDL_Rect {
    int x;
    int y;
    int w;
    int h;
} SDL_Rect;

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

typedef struct SDL_WindowEvent {
    Uint8 event;
} SDL_WindowEvent;

typedef struct SDL_MouseButtonEvent {
    Uint8 button;
    int x;
    int y;
} SDL_MouseButtonEvent;

typedef struct SDL_Event {
    Uint32 type;
    SDL_KeyboardEvent key;
    SDL_WindowEvent window;
    SDL_MouseButtonEvent button;
} SDL_Event;

#define SDL_INIT_VIDEO 0x00000020u
#define SDL_QUIT 0x00000100u
#define SDL_KEYDOWN 0x00000300u
#define SDL_KEYUP 0x00000301u
#define SDL_WINDOWEVENT 0x00000200u
#define SDL_WINDOWEVENT_SIZE_CHANGED 0x05u

#define SDL_MOUSEBUTTONDOWN 0x00000401u

#define SDL_BUTTON_LEFT 1

#define SDLK_ESCAPE 27
#define SDLK_w 'w'
#define SDLK_a 'a'
#define SDLK_s 's'
#define SDLK_d 'd'
#define SDLK_q 'q'
#define SDLK_1 '1'
#define SDLK_2 '2'
#define SDLK_3 '3'
#define SDLK_TAB '\t'
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
SDL_Surface* SDL_LoadBMP(const char* file);
void SDL_FreeSurface(SDL_Surface* surface);
SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer* renderer, SDL_Surface* surface);
void SDL_DestroyTexture(SDL_Texture* texture);

void SDL_GetWindowSize(SDL_Window* window, int* w, int* h);
int SDL_PollEvent(SDL_Event* event);
void SDL_SetWindowTitle(SDL_Window* window, const char* title);

int SDL_SetRenderDrawColor(SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int SDL_RenderClear(SDL_Renderer* renderer);
int SDL_RenderDrawLine(SDL_Renderer* renderer, int x1, int y1, int x2, int y2);
int SDL_RenderDrawLines(SDL_Renderer* renderer, const SDL_Point* points, int count);
int SDL_RenderCopy(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_Rect* srcRect, const SDL_Rect* dstRect);
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

typedef enum RunMode {
    RUNMODE_SEQ = 0,
    RUNMODE_OPENMP = 1,
} RunMode;

typedef enum GameMode {
    GAMEMODE_PEACEFUL = 0,
    GAMEMODE_SURVIVAL = 1,
    GAMEMODE_TERMINATE44 = 2,
} GameMode;

typedef struct AppConfig {
    int width;
    int height;
    int boidCount;
    int threadCount;
    RunMode mode;
    GameMode gameMode;
    bool benchmarkMode;
    bool benchmarkCompare;
    bool liveBenchmarkSession;
    int benchmarkSteps;
    int benchmarkWarmup;
} AppConfig;

typedef struct BenchmarkResult {
    double totalMs;
    double avgMs;
    double ticksPerSecond;
} BenchmarkResult;

enum {
    BENCHMARK_WARMUP_STEPS = 100,
};

typedef struct LiveBenchmarkState {
    bool enabled;
    uint64_t startUs;
    uint64_t lastLogUs;
    double intervalMsSum;
    int intervalTickCount;
} LiveBenchmarkState;

typedef struct DropdownLayout {
    int buttonX;
    int buttonY;
    int buttonW;
    int buttonH;
    int listX;
    int listY;
    int listW;
    int listH;
    int itemH;
} DropdownLayout;

typedef struct ViewTransform {
    float scale;
    float offsetX;
    float offsetY;
    float triSize;
} ViewTransform;

typedef struct ShockwaveSettings {
    float radius;
    float strength;
} ShockwaveSettings;

typedef struct StatsPanelLayout {
    int x;
    int y;
    int w;
    int h;
} StatsPanelLayout;

typedef struct AbilityBarLayout {
    int x;
    int y;
    int w;
    int h;
    int slotSize;
    int slotGap;
    int slotCount;
} AbilityBarLayout;

typedef struct HealthBarLayout {
    int x;
    int y;
    int w;
    int h;
} HealthBarLayout;

typedef struct Glyph5x7 {
    char ch;
    uint8_t rows[7];
} Glyph5x7;

typedef struct UiAssets {
    SDL_Texture* menuButtonClosed;
    SDL_Texture* menuButtonOpen;
    SDL_Texture* menuDropdownPanel;
    SDL_Texture* hpBarEmpty;
    SDL_Texture* hpBarFull;
    SDL_Texture* hpHeartSheet;
    int hpBarW;
    int hpBarH;
    int hpBarFillX;
    int hpBarFillW;
    int hpHeartFrames;
    double hpHeartAnimTime;
} UiAssets;

static void print_usage(const char* exe) {
    printf("Usage: %s [--mode seq|openmp] [--threads N] [--boids N] [--width W] [--height H] [--game peaceful|survival|terminate44]\n", exe);
    printf("       %s --benchmark N [--compare] [--mode seq|openmp] [--threads N] [--boids N] [--width W] [--height H] [--game peaceful|survival|terminate44]\n", exe);
    printf("Controls (in window): WASD move player, Q or ESC quit\n");
}

static bool parse_game_mode(const char* s, GameMode* outMode) {
    if (!s || !outMode) return false;
    if (strcmp(s, "peaceful") == 0) {
        *outMode = GAMEMODE_PEACEFUL;
        return true;
    }
    if (strcmp(s, "survival") == 0) {
        *outMode = GAMEMODE_SURVIVAL;
        return true;
    }
    if (strcmp(s, "terminate44") == 0 || strcmp(s, "terminate") == 0) {
        *outMode = GAMEMODE_TERMINATE44;
        return true;
    }
    return false;
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

    int baseWorldW;
    int baseWorldH;
    float targetPixelsPerUnit;

    InputState input;
    Vec2 playerDir;
    bool shockRequest;
    bool quit;

    double avgMs;
    int avgCount;
    LiveBenchmarkState liveBenchmark;

    SDL_Window* window;
    SDL_Renderer* renderer;
    UiAssets ui;
    int winW;
    int winH;
    int titleCounter;

    GameMode gameMode;
    bool menuOpen;
    bool pendingModeChange;
    GameMode pendingMode;
    double shockCooldown;
    double shockTime;
    float shockRadius;
    int terminateKills;
    double survivalTime;
    bool showStatsPanel;
    int playerHp;
    int playerMaxHp;
    double playerDamageCooldown;
} AppState;

static FILE* g_benchmarkLogFile = NULL;
static char g_benchmarkLogPath[260] = {0};

static void set_group_color(SDL_Renderer* r, unsigned char group, int groupCount);

static float torus_delta_f(float d, float size) {
    if (size <= 0.0f) return d;
    float h = 0.5f * size;
    if (d > h) d -= size;
    else if (d < -h) d += size;
    return d;
}

static const char* run_mode_name(RunMode mode) {
    return mode == RUNMODE_SEQ ? "seq" : "openmp";
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
    if (g_benchmarkLogFile) return true;

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

    g_benchmarkLogFile = fopen(g_benchmarkLogPath, "w");
    return g_benchmarkLogFile != NULL;
}

static void benchmark_close_log_file(void) {
    if (!g_benchmarkLogFile) return;
    fclose(g_benchmarkLogFile);
    g_benchmarkLogFile = NULL;
}

static const char* benchmark_log_path(void) {
    return g_benchmarkLogPath[0] ? g_benchmarkLogPath : NULL;
}

static void benchmark_write_text(const AppConfig* cfg, const char* text) {
    (void)cfg;

    if (!cfg || !text) return;

    fputs(text, stdout);
    fflush(stdout);

    if (g_benchmarkLogFile) {
        fputs(text, g_benchmarkLogFile);
        fflush(g_benchmarkLogFile);
    }
}

static void benchmark_printf(const AppConfig* cfg, const char* fmt, ...) {
    char text[768];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);

    benchmark_write_text(cfg, text);
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

#ifdef _WIN32
enum {
    STARTUP_ID_BOIDS = 1001,
    STARTUP_ID_THREADS = 1002,
    STARTUP_ID_STEPS = 1003,
    STARTUP_ID_START = 1005,
    STARTUP_ID_CANCEL = 1006,
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
    return CreateWindowExW(0, L"STATIC", text,
                           WS_CHILD | WS_VISIBLE,
                           x, y, w, h,
                           parent, NULL, GetModuleHandleW(NULL), NULL);
}

static HWND startup_create_edit(HWND parent, int id, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"EDIT", text,
                           WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
                           x, y, w, h,
                           parent, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
}

static HWND startup_create_button(HWND parent, int id, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"BUTTON", text,
                           WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           x, y, w, h,
                           parent, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
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
    next.mode = next.threadCount <= 1 ? RUNMODE_SEQ : RUNMODE_OPENMP;
    next.benchmarkMode = false;
    next.benchmarkCompare = false;
    next.liveBenchmarkSession = false;

    if (state->benchmarkPrompt) {
        next.benchmarkSteps = startup_read_edit_int(state->stepsEdit, next.benchmarkSteps);
        next.benchmarkWarmup = BENCHMARK_WARMUP_STEPS;
        next.benchmarkCompare = true;
        next.liveBenchmarkSession = true;
        next.mode = next.threadCount <= 1 ? RUNMODE_SEQ : RUNMODE_OPENMP;
    }

    if (next.boidCount <= 0 || next.threadCount <= 0) {
        MessageBoxW(hwnd, L"A boidok és a szálak száma legyen pozitív.", L"Hibás adat", MB_ICONERROR | MB_OK);
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
                             state->benchmarkPrompt ? L"Add meg a mérési indítási adatokat." : L"Add meg a játék indítási adatait.",
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
    const wchar_t* className = L"OpenMPBoidsStartupPromptWindow";
    StartupPromptState state;
    WNDCLASSW wc;
    HWND hwnd;
    MSG msg;
    RECT clientRect;
    const DWORD windowStyle = WS_CAPTION | WS_SYSMENU;
    const DWORD windowExStyle = WS_EX_DLGMODALFRAME;
    int width;
    int height;
    int clientWidth = 340;
    int clientHeight = benchmarkPrompt ? 210 : 180;
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
    clientRect.right = clientWidth;
    clientRect.bottom = clientHeight;
    AdjustWindowRectEx(&clientRect, windowStyle, FALSE, windowExStyle);
    width = clientRect.right - clientRect.left;
    height = clientRect.bottom - clientRect.top;
    x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    hwnd = CreateWindowExW(windowExStyle,
                           className,
                           benchmarkPrompt ? L"OpenMP boids mérés indítás" : L"OpenMP boids játék indítás",
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
#else
static bool prompt_startup_config(AppConfig* cfg, bool benchmarkPrompt) {
    (void)cfg;
    (void)benchmarkPrompt;
    return false;
}
#endif

static const char* game_mode_name(GameMode m) {
    switch (m) {
    case GAMEMODE_PEACEFUL: return "peaceful";
    case GAMEMODE_SURVIVAL: return "survival";
    case GAMEMODE_TERMINATE44: return "terminate 44";
    default: return "?";
    }
}

static const char* game_mode_label(GameMode m) {
    switch (m) {
    case GAMEMODE_PEACEFUL: return "PEACEFUL";
    case GAMEMODE_SURVIVAL: return "SURVIVAL";
    case GAMEMODE_TERMINATE44: return "TERMINATE44";
    default: return "?";
    }
}

static void draw_rect_outline(SDL_Renderer* r, int x, int y, int w, int h) {
    SDL_RenderDrawLine(r, x, y, x + w, y);
    SDL_RenderDrawLine(r, x + w, y, x + w, y + h);
    SDL_RenderDrawLine(r, x + w, y + h, x, y + h);
    SDL_RenderDrawLine(r, x, y + h, x, y);
}

static void draw_rect_filled(SDL_Renderer* r, int x, int y, int w, int h) {
    for (int yy = 0; yy <= h; yy++) {
        SDL_RenderDrawLine(r, x, y + yy, x + w, y + yy);
    }
}

static const Glyph5x7* find_glyph_5x7(char c) {
    static const Glyph5x7 glyphs[] = {
        {' ', {0, 0, 0, 0, 0, 0, 0}},
        {'.', {0, 0, 0, 0, 0, 0x0C, 0x0C}},
        {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
        {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
        {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
        {'3', {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E}},
        {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
        {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
        {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
        {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
        {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
        {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
        {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
        {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
        {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
        {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
        {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
        {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
        {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
        {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
        {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
        {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
        {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
        {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
        {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
        {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
        {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
        {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
        {'?', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}},
    };
    const size_t glyphCount = sizeof(glyphs) / sizeof(glyphs[0]);

    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');

    for (size_t i = 0; i < glyphCount; i++) {
        if (glyphs[i].ch == c) return &glyphs[i];
    }

    return &glyphs[glyphCount - 1];
}

static void draw_text_5x7(SDL_Renderer* r, int x, int y, const char* text, int scale) {
    if (!r || !text) return;
    if (scale < 1) scale = 1;
    int cx = x;
    for (const char* p = text; *p; p++) {
        const Glyph5x7* g = find_glyph_5x7(*p);
        for (int row = 0; row < 7; row++) {
            uint8_t bits = g->rows[row];
            for (int col = 0; col < 5; col++) {
                bool on = (bits & (1u << (4 - col))) != 0;
                if (!on) continue;
                int px = cx + col * scale;
                int py = y + row * scale;
                draw_rect_filled(r, px, py, scale - 1, scale - 1);
            }
        }
        cx += (5 + 1) * scale;
    }
}

static void mode_fill_color(SDL_Renderer* r, GameMode mode) {
    if (mode == GAMEMODE_PEACEFUL) SDL_SetRenderDrawColor(r, 70, 210, 70, 255);
    else if (mode == GAMEMODE_SURVIVAL) SDL_SetRenderDrawColor(r, 230, 200, 70, 255);
    else SDL_SetRenderDrawColor(r, 230, 70, 70, 255);
}

static void draw_mode_icon(SDL_Renderer* r, int cx, int cy, GameMode mode) {
    if (mode == GAMEMODE_PEACEFUL) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_Point pts[4] = {
            {cx - 4, cy + 3},
            {cx + 5, cy},
            {cx - 4, cy - 3},
            {cx - 4, cy + 3},
        };
        SDL_RenderDrawLines(r, pts, 4);
        SDL_Point pts2[4] = {
            {cx + 1, cy + 5},
            {cx + 9, cy + 2},
            {cx + 1, cy - 1},
            {cx + 1, cy + 5},
        };
        SDL_RenderDrawLines(r, pts2, 4);
    } else if (mode == GAMEMODE_SURVIVAL) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_Point pts[4] = {
            {cx - 5, cy + 4},
            {cx + 7, cy},
            {cx - 5, cy - 4},
            {cx - 5, cy + 4},
        };
        SDL_RenderDrawLines(r, pts, 4);
    } else {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderDrawLine(r, cx - 6, cy - 6, cx + 6, cy + 6);
        SDL_RenderDrawLine(r, cx - 6, cy + 6, cx + 6, cy - 6);
    }
}

static DropdownLayout get_mode_dropdown_layout(void) {
    DropdownLayout layout;
    layout.buttonX = 10;
    layout.buttonY = 10;
    layout.buttonW = 128;
    layout.buttonH = 26;
    layout.listX = layout.buttonX;
    layout.listY = layout.buttonY + layout.buttonH + 6;
    layout.listW = layout.buttonW;
    layout.itemH = 24;
    layout.listH = layout.itemH * 3;
    return layout;
}

static StatsPanelLayout get_stats_panel_layout(int windowWidth) {
    DropdownLayout dropdown = get_mode_dropdown_layout();
    StatsPanelLayout layout;
    layout.w = dropdown.buttonW;
    layout.h = 58;
    layout.x = windowWidth - layout.w - dropdown.buttonX;
    layout.y = dropdown.buttonY;
    return layout;
}

static HealthBarLayout get_health_bar_layout(int windowHeight) {
    HealthBarLayout layout;
    layout.w = 180;
    layout.h = 56;
    layout.x = 18;
    layout.y = windowHeight - layout.h - 18;
    return layout;
}

static AbilityBarLayout get_ability_bar_layout(int windowWidth, int windowHeight) {
    AbilityBarLayout layout;
    layout.slotSize = 38;
    layout.slotGap = 8;
    layout.slotCount = 4;
    layout.w = layout.slotCount * layout.slotSize + (layout.slotCount - 1) * layout.slotGap + 20;
    layout.h = layout.slotSize + 18;
    layout.x = (windowWidth - layout.w) / 2;
    layout.y = windowHeight - layout.h - 18;
    return layout;
}

static bool pt_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && y >= ry && x < (rx + rw) && y < (ry + rh);
}

static void draw_mode_dropdown_fallback(SDL_Renderer* r, GameMode current, bool open) {
    DropdownLayout layout = get_mode_dropdown_layout();

    SDL_SetRenderDrawColor(r, 40, 40, 40, 255);
    draw_rect_filled(r, layout.buttonX, layout.buttonY, layout.buttonW, layout.buttonH);
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    draw_rect_outline(r, layout.buttonX, layout.buttonY, layout.buttonW, layout.buttonH);

    mode_fill_color(r, current);
    draw_rect_filled(r, layout.buttonX + 4, layout.buttonY + 4, 18, layout.buttonH - 8);
    draw_mode_icon(r, layout.buttonX + 13, layout.buttonY + layout.buttonH / 2, current);

    SDL_SetRenderDrawColor(r, 235, 235, 235, 255);
    draw_text_5x7(r, layout.buttonX + 28, layout.buttonY + 7, game_mode_label(current), 1);

    SDL_SetRenderDrawColor(r, 220, 220, 220, 255);
    SDL_RenderDrawLine(r, layout.buttonX + layout.buttonW - 18, layout.buttonY + 10, layout.buttonX + layout.buttonW - 10, layout.buttonY + 10);
    SDL_RenderDrawLine(r, layout.buttonX + layout.buttonW - 17, layout.buttonY + 11, layout.buttonX + layout.buttonW - 11, layout.buttonY + 11);
    SDL_RenderDrawLine(r, layout.buttonX + layout.buttonW - 16, layout.buttonY + 12, layout.buttonX + layout.buttonW - 12, layout.buttonY + 12);
    SDL_RenderDrawLine(r, layout.buttonX + layout.buttonW - 15, layout.buttonY + 13, layout.buttonX + layout.buttonW - 13, layout.buttonY + 13);
    SDL_RenderDrawLine(r, layout.buttonX + layout.buttonW - 14, layout.buttonY + 14, layout.buttonX + layout.buttonW - 14, layout.buttonY + 14);

    if (!open) return;

    SDL_SetRenderDrawColor(r, 20, 20, 20, 255);
    draw_rect_filled(r, layout.listX, layout.listY, layout.listW, layout.listH);
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    draw_rect_outline(r, layout.listX, layout.listY, layout.listW, layout.listH);

    for (int i = 0; i < 3; i++) {
        GameMode m = (GameMode)i;
        int y = layout.listY + i * layout.itemH;
        mode_fill_color(r, m);
        draw_rect_filled(r, layout.listX + 2, y + 2, 20, layout.itemH - 4);
        draw_mode_icon(r, layout.listX + 12, y + layout.itemH / 2, m);

        SDL_SetRenderDrawColor(r, 235, 235, 235, 255);
        draw_text_5x7(r, layout.listX + 28, y + 8, game_mode_label(m), 1);

        if (m == current) {
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            draw_rect_outline(r, layout.listX + 1, y + 1, layout.listW - 2, layout.itemH - 2);
        }
    }
}

static void draw_mode_dropdown(AppState* s) {
    draw_mode_dropdown_fallback(s->renderer, s->gameMode, s->menuOpen);
}

static double survival_score_from_time(double survivalTime) {
    return survivalTime / 10.0;
}

static float shock_cooldown_ratio(const AppState* s) {
    const float maxCooldown = 1.0f;
    float value = (float)(s->shockCooldown / (double)maxCooldown);
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    return value;
}

static float player_hp_ratio(const AppState* s) {
    if (s->playerMaxHp <= 0) return 0.0f;
    if (s->playerHp <= 0) return 0.0f;
    if (s->playerHp >= s->playerMaxHp) return 1.0f;
    return (float)s->playerHp / (float)s->playerMaxHp;
}

static SDL_Texture* load_texture_from_bmp(SDL_Renderer* renderer, const char* path) {
    SDL_Surface* surface;
    SDL_Texture* texture;

    surface = SDL_LoadBMP(path);
    if (!surface) return NULL;

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

static bool app_load_ui_assets(AppState* s) {
    s->ui.hpBarEmpty = load_texture_from_bmp(s->renderer, "assets/ui/hp_bar_empty.bmp");
    s->ui.hpBarFull = load_texture_from_bmp(s->renderer, "assets/ui/hp_bar_full.bmp");
    s->ui.hpHeartSheet = load_texture_from_bmp(s->renderer, "assets/ui/hp_heart_sheet.bmp");
    s->ui.hpBarW = 90;
    s->ui.hpBarH = 28;
    s->ui.hpBarFillX = 30;
    s->ui.hpBarFillW = 54;
    s->ui.hpHeartFrames = 2;
    s->ui.hpHeartAnimTime = 0.0;

    if (!s->ui.hpBarEmpty || !s->ui.hpBarFull || !s->ui.hpHeartSheet) {
        if (s->ui.hpBarEmpty) SDL_DestroyTexture(s->ui.hpBarEmpty);
        if (s->ui.hpBarFull) SDL_DestroyTexture(s->ui.hpBarFull);
        if (s->ui.hpHeartSheet) SDL_DestroyTexture(s->ui.hpHeartSheet);
        memset(&s->ui, 0, sizeof(s->ui));
        return false;
    }

    return true;
}

static void app_destroy_ui_assets(AppState* s) {
    if (s->ui.menuButtonClosed) SDL_DestroyTexture(s->ui.menuButtonClosed);
    if (s->ui.menuButtonOpen) SDL_DestroyTexture(s->ui.menuButtonOpen);
    if (s->ui.menuDropdownPanel) SDL_DestroyTexture(s->ui.menuDropdownPanel);
    if (s->ui.hpBarEmpty) SDL_DestroyTexture(s->ui.hpBarEmpty);
    if (s->ui.hpBarFull) SDL_DestroyTexture(s->ui.hpBarFull);
    if (s->ui.hpHeartSheet) SDL_DestroyTexture(s->ui.hpHeartSheet);
    memset(&s->ui, 0, sizeof(s->ui));
}

static void draw_survival_stats_panel(AppState* s) {
    StatsPanelLayout layout;
    char topValueText[32];
    char scoreText[32];
    const char* topLabel;

    if (!s->showStatsPanel) return;
    if (s->gameMode != GAMEMODE_SURVIVAL && s->gameMode != GAMEMODE_TERMINATE44) return;

    layout = get_stats_panel_layout(s->winW);

    SDL_SetRenderDrawColor(s->renderer, 40, 40, 40, 255);
    draw_rect_filled(s->renderer, layout.x, layout.y, layout.w, layout.h);
    SDL_SetRenderDrawColor(s->renderer, 200, 200, 200, 255);
    draw_rect_outline(s->renderer, layout.x, layout.y, layout.w, layout.h);

    SDL_SetRenderDrawColor(s->renderer, 235, 235, 235, 255);
    topLabel = (s->gameMode == GAMEMODE_SURVIVAL) ? "TIME" : "KILL";
    draw_text_5x7(s->renderer, layout.x + 8, layout.y + 8, topLabel, 1);
    draw_text_5x7(s->renderer, layout.x + 8, layout.y + 32, "PTS", 1);

    if (s->gameMode == GAMEMODE_SURVIVAL) {
        snprintf(topValueText, sizeof(topValueText), "%.1f", s->survivalTime);
        snprintf(scoreText, sizeof(scoreText), "%.1f", survival_score_from_time(s->survivalTime));
    } else {
        snprintf(topValueText, sizeof(topValueText), "%d", s->terminateKills);
        snprintf(scoreText, sizeof(scoreText), "%d", s->terminateKills);
    }

    draw_text_5x7(s->renderer, layout.x + 58, layout.y + 8, topValueText, 1);
    draw_text_5x7(s->renderer, layout.x + 58, layout.y + 32, scoreText, 1);
}

static void draw_health_bar_fallback(AppState* s) {
    HealthBarLayout layout = get_health_bar_layout(s->winH);
    int innerW = layout.w - 12;
    int fillW = (int)(player_hp_ratio(s) * (float)innerW + 0.5f);

    SDL_SetRenderDrawColor(s->renderer, 24, 24, 24, 255);
    draw_rect_filled(s->renderer, layout.x, layout.y, layout.w, layout.h);
    SDL_SetRenderDrawColor(s->renderer, 180, 180, 180, 255);
    draw_rect_outline(s->renderer, layout.x, layout.y, layout.w, layout.h);
    SDL_SetRenderDrawColor(s->renderer, 60, 0, 0, 255);
    draw_rect_filled(s->renderer, layout.x + 6, layout.y + 6, innerW, layout.h / 2 - 6);
    SDL_SetRenderDrawColor(s->renderer, 220, 70, 70, 255);
    draw_rect_filled(s->renderer, layout.x + 6, layout.y + 6, fillW, layout.h / 2 - 6);
    SDL_SetRenderDrawColor(s->renderer, 235, 235, 235, 255);
    draw_text_5x7(s->renderer, layout.x + 8, layout.y + layout.h - 14, "HP", 1);
}

static void draw_health_bar(AppState* s) {
    HealthBarLayout layout = get_health_bar_layout(s->winH);

    if (!s->ui.hpBarEmpty || !s->ui.hpBarFull || !s->ui.hpHeartSheet) {
        draw_health_bar_fallback(s);
        return;
    }

    {
        SDL_Rect dst = {layout.x, layout.y, layout.w, layout.h};
        SDL_Rect srcEmpty = {0, 0, s->ui.hpBarW, s->ui.hpBarH};
        int filledSourceWidth = (int)(player_hp_ratio(s) * (float)s->ui.hpBarFillW + 0.5f);
        SDL_Rect srcFull = {s->ui.hpBarFillX, 0, filledSourceWidth, s->ui.hpBarH};
        SDL_Rect dstFull = {layout.x + s->ui.hpBarFillX * 2, layout.y, filledSourceWidth * 2, layout.h};
        int frameIndex = ((int)(s->ui.hpHeartAnimTime / 0.18)) % s->ui.hpHeartFrames;
        SDL_Rect srcHeart = {frameIndex * s->ui.hpBarW, 0, s->ui.hpBarW, s->ui.hpBarH};

        SDL_RenderCopy(s->renderer, s->ui.hpBarEmpty, &srcEmpty, &dst);
        if (filledSourceWidth > 0) {
            SDL_RenderCopy(s->renderer, s->ui.hpBarFull, &srcFull, &dstFull);
        }
        SDL_RenderCopy(s->renderer, s->ui.hpHeartSheet, &srcHeart, &dst);
    }
}

static void draw_ability_bar(AppState* s) {
    AbilityBarLayout layout;

    layout = get_ability_bar_layout(s->winW, s->winH);

    SDL_SetRenderDrawColor(s->renderer, 18, 18, 18, 255);
    draw_rect_filled(s->renderer, layout.x, layout.y, layout.w, layout.h);
    SDL_SetRenderDrawColor(s->renderer, 125, 125, 125, 255);
    draw_rect_outline(s->renderer, layout.x, layout.y, layout.w, layout.h);

    for (int i = 0; i < layout.slotCount; i++) {
        int slotX = layout.x + 10 + i * (layout.slotSize + layout.slotGap);
        int slotY = layout.y + 8;

        SDL_SetRenderDrawColor(s->renderer, 38, 38, 38, 255);
        draw_rect_filled(s->renderer, slotX, slotY, layout.slotSize, layout.slotSize);
        SDL_SetRenderDrawColor(s->renderer, 110, 110, 110, 255);
        draw_rect_outline(s->renderer, slotX, slotY, layout.slotSize, layout.slotSize);

        if (i == 0) {
            int innerX = slotX + 4;
            int innerY = slotY + 4;
            int innerW = layout.slotSize - 8;
            int innerH = layout.slotSize - 8;
            int barH;

            SDL_SetRenderDrawColor(s->renderer, 45, 82, 120, 255);
            draw_rect_filled(s->renderer, innerX, innerY, innerW, innerH);
            SDL_SetRenderDrawColor(s->renderer, 150, 220, 255, 255);
            SDL_RenderDrawLine(s->renderer, innerX + innerW / 2, innerY + 4, innerX + innerW / 2, innerY + innerH - 4);
            SDL_RenderDrawLine(s->renderer, innerX + 4, innerY + innerH / 2, innerX + innerW - 4, innerY + innerH / 2);

            if (s->shockCooldown > 0.0) {
                barH = (int)(shock_cooldown_ratio(s) * (float)innerH + 0.5f);
                SDL_SetRenderDrawColor(s->renderer, 8, 8, 8, 180);
                draw_rect_filled(s->renderer, innerX, innerY, innerW, barH);
            }

            SDL_SetRenderDrawColor(s->renderer, 235, 235, 235, 255);
            draw_text_5x7(s->renderer, slotX + 4, slotY + layout.slotSize - 10, "1", 1);
        } else {
            char slotLabel[2];
            slotLabel[0] = (char)('1' + i);
            slotLabel[1] = '\0';
            SDL_SetRenderDrawColor(s->renderer, 120, 120, 120, 255);
            draw_text_5x7(s->renderer, slotX + 14, slotY + 14, slotLabel, 1);
        }
    }

    SDL_SetRenderDrawColor(s->renderer, 220, 220, 220, 255);
    draw_text_5x7(s->renderer, layout.x + 14, layout.y + layout.h - 8, "SPACE", 1);
}

static ShockwaveSettings get_shockwave_settings(GameMode mode) {
    ShockwaveSettings settings = {7.0f, 18.0f};
    if (mode == GAMEMODE_SURVIVAL) {
        settings.radius = 12.0f;
        settings.strength = 26.0f;
    } else if (mode == GAMEMODE_TERMINATE44) {
        settings.radius = 10.0f;
        settings.strength = 22.0f;
    }
    return settings;
}

static ViewTransform make_view_transform(const AppState* s) {
    ViewTransform view = {1.0f, 0.0f, 0.0f, 6.0f};
    const float sx = (s->world.width > 0) ? ((float)s->winW / (float)s->world.width) : 1.0f;
    const float sy = (s->world.height > 0) ? ((float)s->winH / (float)s->world.height) : 1.0f;

    view.scale = sx < sy ? sx : sy;
    if (view.scale < 1.0f) view.scale = 1.0f;

    {
        const float worldPixW = (float)s->world.width * view.scale;
        const float worldPixH = (float)s->world.height * view.scale;
        view.offsetX = 0.5f * ((float)s->winW - worldPixW);
        view.offsetY = 0.5f * ((float)s->winH - worldPixH);
    }

    if (view.scale > 0.5f) view.triSize = 0.6f * view.scale;
    if (view.triSize < 4.0f) view.triSize = 4.0f;
    if (view.triSize > 14.0f) view.triSize = 14.0f;
    return view;
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

static void draw_boids(AppState* s, const ViewTransform* view) {
    for (size_t i = 0; i < s->world.boidCount; i++) {
        const Boid* boid = &s->world.boids[i];
        float size = view->triSize;
        float px;
        float py;

        if (!boid->alive) continue;

        if (boid->predator) {
            SDL_SetRenderDrawColor(s->renderer, 255, 120, 0, 255);
            size *= 1.8f;
        } else {
            set_group_color(s->renderer, boid->group, s->world.groupCount);
        }

        px = view->offsetX + boid->pos.x * view->scale;
        py = view->offsetY + boid->pos.y * view->scale;
        draw_triangle_boid(s->renderer, px, py, boid->vel, size);
    }
}

static void draw_player(AppState* s, const ViewTransform* view) {
    float px = view->offsetX + s->world.player.pos.x * view->scale;
    float py = view->offsetY + s->world.player.pos.y * view->scale;
    SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255);
    draw_triangle_boid(s->renderer, px, py, s->playerDir, view->triSize);
}

static void draw_shockwave_ring(AppState* s, const ViewTransform* view) {
    enum { SHOCK_RING_SEGS = 24 };
    SDL_Point pts[SHOCK_RING_SEGS + 1];
    int cx;
    int cy;
    int rr;

    if (s->shockTime <= 0.0 || s->shockRadius <= 0.0f) return;

    SDL_SetRenderDrawColor(s->renderer, 120, 200, 255, 255);
    cx = (int)(view->offsetX + s->world.player.pos.x * view->scale + 0.5f);
    cy = (int)(view->offsetY + s->world.player.pos.y * view->scale + 0.5f);
    rr = (int)(s->shockRadius * view->scale + 0.5f);

    for (int i = 0; i <= SHOCK_RING_SEGS; i++) {
        float angle = (float)i * (6.2831853f / (float)SHOCK_RING_SEGS);
        pts[i].x = cx + (int)(cosf(angle) * (float)rr);
        pts[i].y = cy + (int)(sinf(angle) * (float)rr);
    }

    SDL_RenderDrawLines(s->renderer, pts, SHOCK_RING_SEGS + 1);
}

static float frand01_local(void) {
    return (float)rand() / (float)RAND_MAX;
}

static Vec2 rand_unit_dir(void) {
    float a = frand01_local() * 6.2831853f;
    return (Vec2){cosf(a), sinf(a)};
}

static void app_reset_world_for_mode(AppState* s) {
    if (!s) return;
    World tmp;
    if (!world_init(&tmp, s->cfg.width, s->cfg.height, (size_t)s->cfg.boidCount)) {
        return;
    }
    world_destroy(&s->world);
    s->world = tmp;

    s->shockCooldown = 0.0;
    s->shockTime = 0.0;
    s->shockRadius = 0.0f;
    s->terminateKills = 0;
    s->survivalTime = 0.0;
    s->shockRequest = false;
    s->playerDir = (Vec2){1.0f, 0.0f};
    s->playerHp = s->playerMaxHp;
    s->playerDamageCooldown = 0.0;

    if (s->gameMode == GAMEMODE_SURVIVAL) {
        int predCount = 6;
        if ((int)s->world.boidCount < predCount) predCount = (int)s->world.boidCount;
        for (int i = 0; i < predCount; i++) {
            Boid* b = &s->world.boids[i];
            b->predator = 1;
            b->alive = 1;

            const float px = s->world.player.pos.x;
            const float py = s->world.player.pos.y;
            const float ww = (float)s->world.width;
            const float hh = (float)s->world.height;
            Vec2 corners[4] = {
                {2.0f, 2.0f},
                {ww - 3.0f, 2.0f},
                {2.0f, hh - 3.0f},
                {ww - 3.0f, hh - 3.0f},
            };
            Vec2 c = corners[i % 4];
            b->pos = c;
            (void)px;
            (void)py;

            Vec2 d = rand_unit_dir();
            b->vel = (Vec2){d.x * 28.0f, d.y * 28.0f};
        }
    }
}

static void apply_shockwave(World* w, double dt, float radius, float strength) {
    if (!w) return;
    if (radius <= 0.0f || strength <= 0.0f) return;
    (void)dt;

    const float ww = (float)w->width;
    const float hh = (float)w->height;
    const float r2 = radius * radius;
    const float eps = 1e-6f;

    for (size_t i = 0; i < w->boidCount; i++) {
        Boid* b = &w->boids[i];
        if (!b->alive) continue;

        float dx = torus_delta_f(b->pos.x - w->player.pos.x, ww);
        float dy = torus_delta_f(b->pos.y - w->player.pos.y, hh);
        float d2 = dx * dx + dy * dy;
        if (d2 >= r2 || d2 < eps) continue;

        float d = sqrtf(d2);
        float t = (radius - d) / radius;
        float k = (strength * t);
        b->vel.x += (dx / d) * k;
        b->vel.y += (dy / d) * k;
    }
}

static void app_update_world_bounds_for_window(AppState* s) {
    if (!s || !s->window) return;
    if (s->targetPixelsPerUnit <= 0.5f) s->targetPixelsPerUnit = 10.0f;

    int cw = 0;
    int ch = 0;
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
        if (s->world.width > 0) {
            float w = (float)s->world.width;
            while (s->world.player.pos.x >= w) s->world.player.pos.x -= w;
            while (s->world.player.pos.x < 0) s->world.player.pos.x += w;
        }
        if (s->world.height > 0) {
            float h = (float)s->world.height;
            while (s->world.player.pos.y >= h) s->world.player.pos.y -= h;
            while (s->world.player.pos.y < 0) s->world.player.pos.y += h;
        }
    }
}

static void input_set_key(InputState* in, SDL_Keycode key, bool down) {
    if (key == SDLK_w) in->up = down;
    if (key == SDLK_s) in->down = down;
    if (key == SDLK_a) in->left = down;
    if (key == SDLK_d) in->right = down;
}

static void set_group_color(SDL_Renderer* r, unsigned char group, int groupCount) {
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
    SDL_SetRenderDrawColor(r, palette[idx][0], palette[idx][1], palette[idx][2], 255);
}

static void draw_world_sdl(AppState* s) {
    ViewTransform view;

    if (!s || !s->renderer || !s->window) return;

    SDL_GetWindowSize(s->window, &s->winW, &s->winH);
    app_update_world_bounds_for_window(s);
    view = make_view_transform(s);

    SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255);
    SDL_RenderClear(s->renderer);

    draw_mode_dropdown(s);
    draw_survival_stats_panel(s);
    draw_health_bar(s);

    draw_boids(s, &view);
    draw_player(s, &view);
    draw_shockwave_ring(s, &view);
    draw_ability_bar(s);

    SDL_RenderPresent(s->renderer);
}

static AppState app_make_initial_state(AppConfig cfg) {
    AppState state;
    memset(&state, 0, sizeof(state));
    state.cfg = cfg;
    state.baseWorldW = cfg.width;
    state.baseWorldH = cfg.height;
    state.targetPixelsPerUnit = 10.0f;
    state.playerDir = (Vec2){1.0f, 0.0f};
    state.gameMode = cfg.gameMode;
    state.pendingMode = state.gameMode;
    state.showStatsPanel = true;
    state.playerMaxHp = 10;
    state.playerHp = state.playerMaxHp;
    return state;
}

static bool app_create_world(AppState* s) {
    if (!world_init(&s->world, s->cfg.width, s->cfg.height, (size_t)s->cfg.boidCount)) {
        fprintf(stderr, "world_init failed\n");
        return false;
    }

    app_reset_world_for_mode(s);
    return true;
}

static bool app_create_window_and_renderer(AppState* s) {
    const int scale = 10;
    const int winW = s->cfg.width * scale;
    const int winH = s->cfg.height * scale;

    s->window = SDL_CreateWindow(
        "Boids (OpenMP)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        winW,
        winH,
        SDL_WINDOW_RESIZABLE);

    if (!s->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    s->renderer = SDL_CreateRenderer(s->window, -1, SDL_RENDERER_ACCELERATED);
    if (!s->renderer) {
        s->renderer = SDL_CreateRenderer(s->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!s->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(s->window);
        s->window = NULL;
        return false;
    }

    (void)app_load_ui_assets(s);

    return true;
}

static void app_destroy(AppState* s) {
    app_destroy_ui_assets(s);
    if (s->renderer) SDL_DestroyRenderer(s->renderer);
    if (s->window) SDL_DestroyWindow(s->window);
    world_destroy(&s->world);
}

static void app_handle_mouse_click(AppState* s, const SDL_MouseButtonEvent* button) {
    DropdownLayout layout;
    int index;

    if (button->button != SDL_BUTTON_LEFT) return;

    layout = get_mode_dropdown_layout();
    if (pt_in_rect(button->x, button->y, layout.buttonX, layout.buttonY, layout.buttonW, layout.buttonH)) {
        s->menuOpen = !s->menuOpen;
        return;
    }

    if (s->menuOpen && pt_in_rect(button->x, button->y, layout.listX, layout.listY, layout.listW, layout.listH)) {
        index = (button->y - layout.listY) / layout.itemH;
        if (index < 0) index = 0;
        if (index > 2) index = 2;
        s->pendingMode = (GameMode)index;
        s->pendingModeChange = true;
    }

    s->menuOpen = false;
}

static void app_handle_key(AppState* s, SDL_Keycode key, bool down) {
    input_set_key(&s->input, key, down);

    if (!down) return;

    if (key == SDLK_TAB) {
        s->showStatsPanel = !s->showStatsPanel;
        return;
    }

    if (key == SDLK_SPACE || key == ' ') {
        s->shockRequest = true;
    }

    if (key == SDLK_ESCAPE || key == SDLK_q) {
        s->quit = true;
    }
}

static void app_handle_event(AppState* s, const SDL_Event* e) {
    if (e->type == SDL_QUIT) {
        s->quit = true;
    } else if (e->type == SDL_MOUSEBUTTONDOWN) {
        app_handle_mouse_click(s, &e->button);
    } else if (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) {
        app_handle_key(s, e->key.keysym.sym, e->type == SDL_KEYDOWN);
    } else if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        app_update_world_bounds_for_window(s);
    }
}

static void app_poll_events(AppState* s) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        app_handle_event(s, &e);
        if (s->quit) break;
    }
}

static void app_apply_pending_mode_change(AppState* s) {
    if (!s->pendingModeChange) return;
    s->pendingModeChange = false;
    s->gameMode = s->pendingMode;
    app_reset_world_for_mode(s);
}

static void app_update_player_direction(AppState* s) {
    Vec2 moveDir = {0, 0};
    if (s->input.up) moveDir.y -= 1;
    if (s->input.down) moveDir.y += 1;
    if (s->input.left) moveDir.x -= 1;
    if (s->input.right) moveDir.x += 1;

    if (moveDir.x != 0 || moveDir.y != 0) {
        s->playerDir = normalize_or_default(moveDir, s->playerDir);
    }
}

static void app_update_shockwave(AppState* s, double simDt) {
    ShockwaveSettings settings = get_shockwave_settings(s->gameMode);

    if (s->shockCooldown > 0.0) s->shockCooldown -= simDt;
    if (s->shockTime > 0.0) s->shockTime -= simDt;

    if (s->shockRequest) {
        if (s->shockCooldown <= 0.0 && s->shockTime <= 0.0) {
            s->shockTime = 0.18;
            s->shockCooldown = 1.00;
            s->shockRadius = settings.radius;
            apply_shockwave(&s->world, simDt, settings.radius, settings.strength);
        }
        s->shockRequest = false;
    }

    if (s->shockTime > 0.0) {
        apply_shockwave(&s->world, simDt, s->shockRadius, settings.strength);
    }

    s->ui.hpHeartAnimTime += simDt;
}

static void app_step_boids_seq(AppState* s, double simDt) {
    (void)world_step_seq(&s->world, simDt);
}

static void app_step_boids_openmp(AppState* s, double simDt) {
    (void)world_step_openmp(&s->world, s->cfg.threadCount, simDt);
}

static void app_step_boids(AppState* s, double simDt) {
    if (s->cfg.mode == RUNMODE_SEQ) {
        app_step_boids_seq(s, simDt);
    } else {
        app_step_boids_openmp(s, simDt);
    }
}

static BenchmarkResult app_run_benchmark(AppState* s, int warmupSteps, int measureSteps, double simDt) {
    BenchmarkResult result = {0};

    for (int i = 0; i < warmupSteps; i++) {
        app_step_boids(s, simDt);
    }

    {
        uint64_t t0 = time_now_us();
        for (int i = 0; i < measureSteps; i++) {
            app_step_boids(s, simDt);
        }
        uint64_t t1 = time_now_us();
        result.totalMs = (double)(t1 - t0) / 1000.0;
    }

    if (measureSteps > 0) {
        result.avgMs = result.totalMs / (double)measureSteps;
    }
    if (result.totalMs > 0.0) {
        result.ticksPerSecond = (double)measureSteps * 1000.0 / result.totalMs;
    }

    return result;
}

static bool app_prepare_benchmark_state(AppState* s, AppConfig cfg, unsigned seed) {
    *s = app_make_initial_state(cfg);
    srand(seed);
    return app_create_world(s);
}

static void print_benchmark_result(const AppConfig* cfg, const BenchmarkResult* result) {
    char text[512];

    snprintf(text, sizeof(text),
             "benchmark mode=%s game=%s section=world_update_only threads=%d boids=%d size=%dx%d steps=%d total=%.3f ms avg=%.3f ms/tick ticks=%.2f/s\n",
             run_mode_name(cfg->mode),
             game_mode_name(cfg->gameMode),
             cfg->threadCount,
             cfg->boidCount,
             cfg->width,
             cfg->height,
             cfg->benchmarkSteps,
             result->totalMs,
             result->avgMs,
             result->ticksPerSecond);

    benchmark_write_text(cfg, text);
}

static int run_single_benchmark(const AppConfig* cfg, double simDt) {
    const unsigned benchmarkSeed = 12345u;
    AppState state;
    BenchmarkResult result;

    if (!app_prepare_benchmark_state(&state, *cfg, benchmarkSeed)) {
        return 1;
    }

    result = app_run_benchmark(&state, cfg->benchmarkWarmup, cfg->benchmarkSteps, simDt);
    print_benchmark_result(cfg, &result);
    app_destroy(&state);
    return 0;
}

static int run_compare_benchmark(const AppConfig* cfg, double simDt) {
    const unsigned benchmarkSeed = 12345u;
    AppConfig seqCfg = *cfg;
    AppConfig openmpCfg = *cfg;
    AppState seqState;
    AppState openmpState;
    BenchmarkResult seqResult;
    BenchmarkResult openmpResult;
    double speedup = 0.0;
    char text[768];

    seqCfg.mode = RUNMODE_SEQ;
    seqCfg.threadCount = 1;
    openmpCfg.mode = RUNMODE_OPENMP;

    if (!app_prepare_benchmark_state(&seqState, seqCfg, benchmarkSeed)) {
        return 1;
    }
    if (!app_prepare_benchmark_state(&openmpState, openmpCfg, benchmarkSeed)) {
        app_destroy(&seqState);
        return 1;
    }

    seqResult = app_run_benchmark(&seqState, seqCfg.benchmarkWarmup, seqCfg.benchmarkSteps, simDt);
    openmpResult = app_run_benchmark(&openmpState, openmpCfg.benchmarkWarmup, openmpCfg.benchmarkSteps, simDt);

    if (openmpResult.avgMs > 0.0) {
        speedup = seqResult.avgMs / openmpResult.avgMs;
    }

    snprintf(text, sizeof(text),
             "benchmark compare game=%s section=world_update_only boids=%d size=%dx%d steps=%d\n"
             "  seq:        avg=%.3f ms/tick | ticks=%.2f/s\n"
             "  openmp(%d): avg=%.3f ms/tick | ticks=%.2f/s\n"
             "  speedup:    %.2fx\n",
             game_mode_name(cfg->gameMode),
             cfg->boidCount,
             cfg->width,
             cfg->height,
             cfg->benchmarkSteps,
             seqResult.avgMs,
             seqResult.ticksPerSecond,
             openmpCfg.threadCount,
             openmpResult.avgMs,
             openmpResult.ticksPerSecond,
             speedup);

    benchmark_write_text(cfg, text);

    app_destroy(&seqState);
    app_destroy(&openmpState);
    return 0;
}

static void app_init_live_benchmark(AppState* s) {
    if (!s) return;

    memset(&s->liveBenchmark, 0, sizeof(s->liveBenchmark));
    if (!s->cfg.liveBenchmarkSession) return;

    s->liveBenchmark.enabled = true;
    s->liveBenchmark.startUs = time_now_us();
    s->liveBenchmark.lastLogUs = s->liveBenchmark.startUs;

    benchmark_printf(&s->cfg,
                     "interactive benchmark start game=%s run=%s boids=%d threads=%d\n",
                     game_mode_name(s->gameMode),
                     run_mode_name(s->cfg.mode),
                     s->cfg.boidCount,
                     s->cfg.threadCount);
}

static void app_note_live_benchmark_step(AppState* s, double stepMs) {
    if (!s || !s->liveBenchmark.enabled) return;

    s->liveBenchmark.intervalMsSum += stepMs;
    s->liveBenchmark.intervalTickCount++;
}

static void app_log_live_benchmark_if_needed(AppState* s) {
    uint64_t nowUs;
    uint64_t intervalUs;
    double totalSec;
    double intervalSec;
    double intervalAvgMs = 0.0;
    double intervalTicksPerSec = 0.0;

    if (!s || !s->liveBenchmark.enabled) return;

    nowUs = time_now_us();
    intervalUs = nowUs - s->liveBenchmark.lastLogUs;
    if (intervalUs < 1000000ULL) return;

    totalSec = (double)(nowUs - s->liveBenchmark.startUs) / 1000000.0;
    intervalSec = (double)intervalUs / 1000000.0;
    if (s->liveBenchmark.intervalTickCount > 0) {
        intervalAvgMs = s->liveBenchmark.intervalMsSum / (double)s->liveBenchmark.intervalTickCount;
        intervalTicksPerSec = (double)s->liveBenchmark.intervalTickCount / intervalSec;
    }

    benchmark_printf(&s->cfg,
                     "[live %.1fs] game=%s run=%s boids=%zu threads=%d interval=%.3f ms/tick overall=%.3f ms/tick ticks=%.2f/s\n",
                     totalSec,
                     game_mode_name(s->gameMode),
                     run_mode_name(s->cfg.mode),
                     s->world.boidCount,
                     s->cfg.threadCount,
                     intervalAvgMs,
                     s->avgMs,
                     intervalTicksPerSec);

    s->liveBenchmark.lastLogUs = nowUs;
    s->liveBenchmark.intervalMsSum = 0.0;
    s->liveBenchmark.intervalTickCount = 0;
}

static void app_finish_live_benchmark(AppState* s) {
    double totalSec;

    if (!s || !s->liveBenchmark.enabled) return;

    totalSec = (double)(time_now_us() - s->liveBenchmark.startUs) / 1000000.0;
    benchmark_printf(&s->cfg,
                     "interactive benchmark end runtime=%.1fs avg=%.3f ms/tick samples=%d log=%s\n",
                     totalSec,
                     s->avgMs,
                     s->avgCount,
                     benchmark_log_path() ? benchmark_log_path() : "-");
}

static void app_apply_survival_rules(AppState* s) {
    const float hitR = 1.6f;
    const float hitR2 = hitR * hitR;
    const float worldW = (float)s->world.width;
    const float worldH = (float)s->world.height;

    for (size_t i = 0; i < s->world.boidCount; i++) {
        const Boid* boid = &s->world.boids[i];
        float dx;
        float dy;
        float d2;

        if (!boid->alive || !boid->predator) continue;

        dx = torus_delta_f(boid->pos.x - s->world.player.pos.x, worldW);
        dy = torus_delta_f(boid->pos.y - s->world.player.pos.y, worldH);
        d2 = dx * dx + dy * dy;
        if (d2 < hitR2 && s->playerDamageCooldown <= 0.0) {
            s->playerHp--;
            s->playerDamageCooldown = 0.75;
            if (s->playerHp <= 0) {
                app_reset_world_for_mode(s);
            }
            return;
        }
    }
}

static void app_apply_terminate_rules(AppState* s) {
    const float killR = 1.4f;
    const float killR2 = killR * killR;
    const float worldW = (float)s->world.width;
    const float worldH = (float)s->world.height;

    for (size_t i = 0; i < s->world.boidCount; i++) {
        Boid* boid = &s->world.boids[i];
        float dx;
        float dy;
        float d2;

        if (!boid->alive || boid->predator) continue;

        dx = torus_delta_f(boid->pos.x - s->world.player.pos.x, worldW);
        dy = torus_delta_f(boid->pos.y - s->world.player.pos.y, worldH);
        d2 = dx * dx + dy * dy;
        if (d2 < killR2) {
            boid->alive = 0;
            s->terminateKills++;
        }
    }
}

static void app_apply_mode_rules(AppState* s) {
    if (s->gameMode == GAMEMODE_SURVIVAL) {
        app_apply_survival_rules(s);
    } else if (s->gameMode == GAMEMODE_TERMINATE44) {
        app_apply_terminate_rules(s);
    }
}

static void app_step_simulation(AppState* s, double simDt) {
    app_update_world_bounds_for_window(s);
    app_apply_pending_mode_change(s);
    app_update_player_direction(s);
    world_apply_player_input(&s->world, &s->input, simDt);
    app_update_shockwave(s, simDt);
    app_step_boids(s, simDt);
    if (s->playerDamageCooldown > 0.0) s->playerDamageCooldown -= simDt;
    if (s->playerDamageCooldown < 0.0) s->playerDamageCooldown = 0.0;
    if (s->gameMode == GAMEMODE_SURVIVAL) {
        s->survivalTime += simDt;
    }
    app_apply_mode_rules(s);
}

static void update_window_title(AppState* s) {
    if (!s || !s->window) return;
    if (++s->titleCounter < 12) return;
    s->titleCounter = 0;
    char title[256];
    const char* gm = game_mode_name(s->gameMode);
    char extra[64] = {0};
    if (s->gameMode == GAMEMODE_TERMINATE44) {
        snprintf(extra, sizeof(extra), " | kills=%d/44", s->terminateKills);
    } else if (s->gameMode == GAMEMODE_SURVIVAL) {
        snprintf(extra, sizeof(extra), " | SURVIVE");
    }

    snprintf(title, sizeof(title),
             "Boids=%zu | run=%s | game=%s%s | threads=%d | avg=%.3f ms/tick | 1/2/3 mode | SPACE shock | WASD | Q/ESC quit",
             s->world.boidCount,
             run_mode_name(s->cfg.mode),
             gm,
             extra,
             s->cfg.threadCount,
             s->avgMs);
    SDL_SetWindowTitle(s->window, title);
}

int main(int argc, char** argv) {
    const bool benchmarkExe = exe_name_is_benchmark(argc > 0 ? argv[0] : NULL);
    AppConfig cfg = {
        .width = 80,
        .height = 25,
        .boidCount = 800,
        .threadCount = 4,
        .mode = RUNMODE_OPENMP,
        .gameMode = GAMEMODE_PEACEFUL,
        .benchmarkMode = false,
        .benchmarkCompare = false,
        .liveBenchmarkSession = false,
        .benchmarkSteps = 0,
        .benchmarkWarmup = BENCHMARK_WARMUP_STEPS,
    };
    const double simDt = 1.0 / 120.0;

    if (argc <= 1) {
        cfg.benchmarkSteps = 500;
        cfg.benchmarkWarmup = BENCHMARK_WARMUP_STEPS;
        if (!prompt_startup_config(&cfg, benchmarkExe)) {
            return 0;
        }
    } else {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--help") == 0) {
                print_usage(argv[0]);
                return 0;
            }
            if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
                const char* m = argv[++i];
                if (strcmp(m, "seq") == 0) cfg.mode = RUNMODE_SEQ;
                else if (strcmp(m, "openmp") == 0) cfg.mode = RUNMODE_OPENMP;
                else {
                    fprintf(stderr, "Unknown mode: %s\n", m);
                    return 2;
                }
                continue;
            }
            if (strcmp(argv[i], "--game") == 0 && i + 1 < argc) {
                if (!parse_game_mode(argv[++i], &cfg.gameMode)) {
                    fprintf(stderr, "Unknown game mode: %s\n", argv[i]);
                    return 2;
                }
                continue;
            }
            if (strcmp(argv[i], "--benchmark") == 0 && i + 1 < argc) {
                cfg.benchmarkMode = true;
                cfg.benchmarkSteps = parse_int(argv[++i], cfg.benchmarkSteps);
                continue;
            }
            if (strcmp(argv[i], "--compare") == 0) {
                cfg.benchmarkMode = true;
                cfg.benchmarkCompare = true;
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
    }

    if (cfg.width <= 10 || cfg.height <= 10 || cfg.boidCount <= 0 || cfg.threadCount <= 0) {
        fprintf(stderr, "Invalid config. Use --help\n");
        return 2;
    }
    if (cfg.benchmarkMode && cfg.benchmarkSteps <= 0) {
        fprintf(stderr, "Benchmark mode needs a positive step count. Use --benchmark N\n");
        return 2;
    }
    cfg.benchmarkWarmup = BENCHMARK_WARMUP_STEPS;

    if (cfg.benchmarkMode || cfg.liveBenchmarkSession) {
        benchmark_attach_console();
    }

    if (SDL_Init(cfg.benchmarkMode ? 0u : SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (cfg.benchmarkMode) {
        int rc = cfg.benchmarkCompare ? run_compare_benchmark(&cfg, simDt) : run_single_benchmark(&cfg, simDt);
        SDL_Quit();
        return rc;
    }

    if (cfg.liveBenchmarkSession) {
        if (!benchmark_open_log_file()) {
            fprintf(stderr, "Warning: could not open benchmark log file for writing.\n");
        }
        benchmark_printf(&cfg,
                         "benchmark launcher config boids=%d threads=%d compare_steps=%d\n",
                         cfg.boidCount,
                         cfg.threadCount,
                         cfg.benchmarkSteps);
        if (benchmark_log_path()) {
            benchmark_printf(&cfg, "benchmark log file: %s\n", benchmark_log_path());
        }
        (void)run_compare_benchmark(&cfg, simDt);
    }

    srand((unsigned)time_now_us());

    AppState st = app_make_initial_state(cfg);

    if (!app_create_world(&st)) {
        benchmark_close_log_file();
        SDL_Quit();
        return 1;
    }

    if (!app_create_window_and_renderer(&st)) {
        app_destroy(&st);
        benchmark_close_log_file();
        SDL_Quit();
        return 1;
    }

    app_init_live_benchmark(&st);

    double acc = 0.0;
    uint64_t lastUs = time_now_us();

    while (!st.quit) {
        app_poll_events(&st);
        if (st.quit) break;

        uint64_t nowUs = time_now_us();
        double frameDt = (double)(nowUs - lastUs) / 1000000.0;
        lastUs = nowUs;
        if (frameDt > 0.05) frameDt = 0.05;
        acc += frameDt;

        while (acc >= simDt) {
            const uint64_t t0 = time_now_us();
            app_step_simulation(&st, simDt);
            const uint64_t t1 = time_now_us();
            const double ms = (double)(t1 - t0) / 1000.0;
            st.avgCount++;
            st.avgMs += (ms - st.avgMs) / (double)st.avgCount;
            app_note_live_benchmark_step(&st, ms);

            acc -= simDt;
        }

        app_log_live_benchmark_if_needed(&st);
        update_window_title(&st);
        draw_world_sdl(&st);
        time_sleep_us(1000);
    }

    app_finish_live_benchmark(&st);
    app_destroy(&st);
    benchmark_close_log_file();
    SDL_Quit();
    return 0;
}