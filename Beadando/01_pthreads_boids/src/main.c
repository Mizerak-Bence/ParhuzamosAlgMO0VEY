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
#if defined(__INTELLISENSE__)
/*
   IntelliSense fallback:
   The project builds via the Makefile (with MSYS2 include paths), but VS Code's
   C/C++ extension may not automatically inherit those include paths.
   These minimal stubs prevent a wall of editor-only errors without affecting
   real compilation.
*/
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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

typedef enum RunMode {
    RUNMODE_SEQ = 0,
    RUNMODE_PTHREAD = 1,
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
    Vec2 playerDir;
    bool shockRequest;
    bool quit;

    double avgMs;
    int avgCount;

    SDL_Window* window;
    SDL_Renderer* renderer;
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
} AppState;

static float torus_delta_f(float d, float size) {
    if (size <= 0.0f) return d;
    float h = 0.5f * size;
    if (d > h) d -= size;
    else if (d < -h) d += size;
    return d;
}

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

static const uint8_t* glyph_5x7(char c) {
    /* Each row is 5 bits (MSB on the left), 7 rows total */
    static const uint8_t SPACE[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t QMARK[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};
    static const uint8_t D0[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    static const uint8_t D1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const uint8_t D2[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    static const uint8_t D3[7] = {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E};
    static const uint8_t D4[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    static const uint8_t D5[7] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
    static const uint8_t D6[7] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    static const uint8_t D7[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    static const uint8_t D8[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    static const uint8_t D9[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};

    static const uint8_t A[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const uint8_t C[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    static const uint8_t E[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    static const uint8_t F[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    static const uint8_t I[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const uint8_t L[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    static const uint8_t M[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    static const uint8_t N[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    static const uint8_t P[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    static const uint8_t R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    static const uint8_t S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    static const uint8_t T[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    static const uint8_t U[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t V[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};

    if (c == ' ') return SPACE;
    if (c >= '0' && c <= '9') {
        switch (c) {
        case '0': return D0;
        case '1': return D1;
        case '2': return D2;
        case '3': return D3;
        case '4': return D4;
        case '5': return D5;
        case '6': return D6;
        case '7': return D7;
        case '8': return D8;
        case '9': return D9;
        default: return QMARK;
        }
    }
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    switch (c) {
    case 'A': return A;
    case 'C': return C;
    case 'E': return E;
    case 'F': return F;
    case 'I': return I;
    case 'L': return L;
    case 'M': return M;
    case 'N': return N;
    case 'P': return P;
    case 'R': return R;
    case 'S': return S;
    case 'T': return T;
    case 'U': return U;
    case 'V': return V;
    default: return QMARK;
    }
}

static void draw_text_5x7(SDL_Renderer* r, int x, int y, const char* text, int scale) {
    if (!r || !text) return;
    if (scale < 1) scale = 1;
    int cx = x;
    for (const char* p = text; *p; p++) {
        const uint8_t* g = glyph_5x7(*p);
        for (int row = 0; row < 7; row++) {
            uint8_t bits = g[row];
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

static void get_mode_dropdown_rects(int* btnX, int* btnY, int* btnW, int* btnH, int* listX, int* listY, int* listW, int* listH, int* itemH) {
    *btnX = 10;
    *btnY = 10;
    *btnW = 128;
    *btnH = 26;

    *listX = *btnX;
    *listY = *btnY + *btnH + 6;
    *listW = *btnW;
    *itemH = 24;
    *listH = (*itemH) * 3;
}

static bool pt_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && y >= ry && x < (rx + rw) && y < (ry + rh);
}

static void draw_mode_dropdown(SDL_Renderer* r, GameMode current, bool open) {
    int bx, by, bw, bh, lx, ly, lw, lh, ih;
    get_mode_dropdown_rects(&bx, &by, &bw, &bh, &lx, &ly, &lw, &lh, &ih);

    SDL_SetRenderDrawColor(r, 40, 40, 40, 255);
    draw_rect_filled(r, bx, by, bw, bh);
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    draw_rect_outline(r, bx, by, bw, bh);

    mode_fill_color(r, current);
    draw_rect_filled(r, bx + 4, by + 4, 18, bh - 8);
    draw_mode_icon(r, bx + 13, by + bh / 2, current);

    /* label in button */
    SDL_SetRenderDrawColor(r, 235, 235, 235, 255);
    draw_text_5x7(r, bx + 28, by + 7, game_mode_label(current), 1);

    SDL_SetRenderDrawColor(r, 220, 220, 220, 255);
    SDL_RenderDrawLine(r, bx + bw - 18, by + 10, bx + bw - 10, by + 10);
    SDL_RenderDrawLine(r, bx + bw - 17, by + 11, bx + bw - 11, by + 11);
    SDL_RenderDrawLine(r, bx + bw - 16, by + 12, bx + bw - 12, by + 12);
    SDL_RenderDrawLine(r, bx + bw - 15, by + 13, bx + bw - 13, by + 13);
    SDL_RenderDrawLine(r, bx + bw - 14, by + 14, bx + bw - 14, by + 14);

    if (!open) return;

    SDL_SetRenderDrawColor(r, 20, 20, 20, 255);
    draw_rect_filled(r, lx, ly, lw, lh);
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    draw_rect_outline(r, lx, ly, lw, lh);

    for (int i = 0; i < 3; i++) {
        GameMode m = (GameMode)i;
        int y = ly + i * ih;
        mode_fill_color(r, m);
        draw_rect_filled(r, lx + 2, y + 2, 20, ih - 4);
        draw_mode_icon(r, lx + 12, y + ih / 2, m);

        SDL_SetRenderDrawColor(r, 235, 235, 235, 255);
        draw_text_5x7(r, lx + 28, y + 8, game_mode_label(m), 1);

        if (m == current) {
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            draw_rect_outline(r, lx + 1, y + 1, lw - 2, ih - 2);
        }
    }
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

    /* reset ability + counters */
    s->shockCooldown = 0.0;
    s->shockTime = 0.0;
    s->shockRadius = 0.0f;
    s->terminateKills = 0;
    s->shockRequest = false;
    s->playerDir = (Vec2){1.0f, 0.0f};

    if (s->gameMode == GAMEMODE_SURVIVAL) {
        int predCount = 6;
        if ((int)s->world.boidCount < predCount) predCount = (int)s->world.boidCount;
        for (int i = 0; i < predCount; i++) {
            Boid* b = &s->world.boids[i];
            b->predator = 1;
            b->alive = 1;

            /* spawn predators away from the player to avoid instant game-over loops */
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
            (void)px; (void)py;

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
        /* impulse (not acceleration): make it feel immediate */
        float k = (strength * t);
        b->vel.x += (dx / d) * k;
        b->vel.y += (dy / d) * k;
    }
}

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
    if (!s || !s->renderer || !s->window) return;

    SDL_GetWindowSize(s->window, &s->winW, &s->winH);
    app_update_world_bounds_for_window(s);

    SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255);
    SDL_RenderClear(s->renderer);

    draw_mode_dropdown(s->renderer, s->gameMode, s->menuOpen);

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

    for (size_t i = 0; i < s->world.boidCount; i++) {
        const Boid* b = &s->world.boids[i];
        if (!b->alive) continue;
        float localTri = triSize;
        if (b->predator) {
            SDL_SetRenderDrawColor(s->renderer, 255, 120, 0, 255);
            localTri = triSize * 1.8f;
        } else {
            set_group_color(s->renderer, b->group, s->world.groupCount);
        }
        float vx = b->vel.x;
        float vy = b->vel.y;
        float vl = sqrtf(vx * vx + vy * vy);
        if (vl < 1e-4f) { vx = 1.0f; vy = 0.0f; vl = 1.0f; }
        vx /= vl;
        vy /= vl;

        float px = ox + b->pos.x * scale;
        float py = oy + b->pos.y * scale;
        float hx = px + vx * localTri;
        float hy = py + vy * localTri;
        float pxp = -vy;
        float pyp = vx;
        float bx = px - vx * (localTri * 0.7f);
        float by = py - vy * (localTri * 0.7f);
        float lx = bx + pxp * (localTri * 0.6f);
        float ly = by + pyp * (localTri * 0.6f);
        float rx = bx - pxp * (localTri * 0.6f);
        float ry = by - pyp * (localTri * 0.6f);

        SDL_Point pts[4];
        pts[0] = (SDL_Point){(int)(hx + 0.5f), (int)(hy + 0.5f)};
        pts[1] = (SDL_Point){(int)(lx + 0.5f), (int)(ly + 0.5f)};
        pts[2] = (SDL_Point){(int)(rx + 0.5f), (int)(ry + 0.5f)};
        pts[3] = pts[0];
        SDL_RenderDrawLines(s->renderer, pts, 4);
    }

    /* Player: fixed unique color */
    SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255);
    {
        Vec2 d = s->playerDir;
        float vl = sqrtf(d.x * d.x + d.y * d.y);
        if (vl < 1e-4f) {
            d = (Vec2){1.0f, 0.0f};
            vl = 1.0f;
        }
        float vx = d.x / vl;
        float vy = d.y / vl;

        float px = ox + s->world.player.pos.x * scale;
        float py = oy + s->world.player.pos.y * scale;
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

    /* shockwave ring for clear feedback */
    if (s->shockTime > 0.0 && s->shockRadius > 0.0f) {
        SDL_SetRenderDrawColor(s->renderer, 120, 200, 255, 255);
        const int cx = (int)(ox + s->world.player.pos.x * scale + 0.5f);
        const int cy = (int)(oy + s->world.player.pos.y * scale + 0.5f);
        const int rr = (int)(s->shockRadius * scale + 0.5f);
        enum { SHOCK_RING_SEGS = 24 };
        SDL_Point pts[SHOCK_RING_SEGS + 1];
        for (int i = 0; i <= SHOCK_RING_SEGS; i++) {
            float a = (float)i * (6.2831853f / (float)SHOCK_RING_SEGS);
            pts[i].x = cx + (int)(cosf(a) * (float)rr);
            pts[i].y = cy + (int)(sinf(a) * (float)rr);
        }
        SDL_RenderDrawLines(s->renderer, pts, SHOCK_RING_SEGS + 1);
    }

    SDL_RenderPresent(s->renderer);
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
             s->cfg.mode == RUNMODE_SEQ ? "seq" : "pthread",
             gm,
             extra,
             s->cfg.threadCount,
             s->avgMs);
    SDL_SetWindowTitle(s->window, title);
}

int main(int argc, char** argv) {
    AppConfig cfg = {
        .width = 80,
        .height = 25,
            .boidCount = 800,
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
    st.playerDir = (Vec2){1.0f, 0.0f};
    st.gameMode = GAMEMODE_PEACEFUL;
    st.menuOpen = false;
    st.pendingModeChange = false;
    st.pendingMode = st.gameMode;

    if (!world_init(&st.world, cfg.width, cfg.height, (size_t)cfg.boidCount)) {
        fprintf(stderr, "world_init failed\n");
        return 1;
    }

    app_reset_world_for_mode(&st);

    if (cfg.mode == RUNMODE_PTHREAD) {
        if (!update_pthreads_init(&st.updater, (size_t)cfg.threadCount)) {
            fprintf(stderr, "update_pthreads_init failed\n");
            world_destroy(&st.world);
            return 1;
        }
        st.updaterInited = true;
    }

    const int scale = 10;
    const int winW = cfg.width * scale;
    const int winH = cfg.height * scale;

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

            if (e.type == SDL_MOUSEBUTTONDOWN) {
                /* dropdown mode selection */
                const int mx = e.button.x;
                const int my = e.button.y;
                const int left = e.button.button;
                if (left == SDL_BUTTON_LEFT) {
                    int bx, by, bw, bh, lx, ly, lw, lh, ih;
                    get_mode_dropdown_rects(&bx, &by, &bw, &bh, &lx, &ly, &lw, &lh, &ih);
                    if (pt_in_rect(mx, my, bx, by, bw, bh)) {
                        st.menuOpen = !st.menuOpen;
                    } else if (st.menuOpen && pt_in_rect(mx, my, lx, ly, lw, lh)) {
                        int idx = (my - ly) / ih;
                        if (idx < 0) idx = 0;
                        if (idx > 2) idx = 2;
                        st.pendingMode = (GameMode)idx;
                        st.pendingModeChange = true;
                        st.menuOpen = false;
                    } else {
                        st.menuOpen = false;
                    }
                }
            }

            if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                const bool down = (e.type == SDL_KEYDOWN);
                SDL_Keycode key = e.key.keysym.sym;
                input_set_key(&st.input, key, down);
                if (down && (key == SDLK_SPACE || key == ' ')) {
                    st.shockRequest = true;
                }
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

            if (st.pendingModeChange) {
                st.pendingModeChange = false;
                st.gameMode = st.pendingMode;
                app_reset_world_for_mode(&st);
            }

            Vec2 moveDir = {0, 0};
            if (st.input.up) moveDir.y -= 1;
            if (st.input.down) moveDir.y += 1;
            if (st.input.left) moveDir.x -= 1;
            if (st.input.right) moveDir.x += 1;
            if (moveDir.x != 0 || moveDir.y != 0) {
                float l = sqrtf(moveDir.x * moveDir.x + moveDir.y * moveDir.y);
                if (l > 1e-6f) st.playerDir = (Vec2){moveDir.x / l, moveDir.y / l};
            }

            world_apply_player_input(&st.world, &st.input, simDt);

            /* shockwave ability */
            if (st.shockCooldown > 0.0) st.shockCooldown -= simDt;
            if (st.shockTime > 0.0) st.shockTime -= simDt;
            if (st.shockRequest && st.shockCooldown <= 0.0) {
                st.shockRequest = false;
                st.shockTime = 0.18;
                st.shockCooldown = 1.00;

                float radius = 7.0f;
                float strength = 18.0f;
                if (st.gameMode == GAMEMODE_SURVIVAL) { radius = 12.0f; strength = 26.0f; }
                if (st.gameMode == GAMEMODE_TERMINATE44) { radius = 10.0f; strength = 22.0f; }
                st.shockRadius = radius;
                apply_shockwave(&st.world, simDt, radius, strength);
            }

            if (st.shockTime > 0.0) {
                float radius = st.shockRadius;
                float strength = 18.0f;
                if (st.gameMode == GAMEMODE_SURVIVAL) strength = 26.0f;
                if (st.gameMode == GAMEMODE_TERMINATE44) strength = 22.0f;
                apply_shockwave(&st.world, simDt, radius, strength);
            }

            const uint64_t t0 = time_now_us();
            if (cfg.mode == RUNMODE_SEQ) {
                world_step_range(&st.world, &st.world, 0, st.world.boidCount, simDt);
                world_swap_buffers(&st.world);
            } else {
                update_pthreads_step(&st.updater, &st.world, simDt);
            }

            /* mode rules that need current positions */
            if (st.gameMode == GAMEMODE_SURVIVAL) {
                const float hitR = 1.6f;
                const float hitR2 = hitR * hitR;
                const float ww = (float)st.world.width;
                const float hh = (float)st.world.height;
                bool dead = false;
                for (size_t i = 0; i < st.world.boidCount; i++) {
                    const Boid* b = &st.world.boids[i];
                    if (!b->alive || !b->predator) continue;
                    float dx = torus_delta_f(b->pos.x - st.world.player.pos.x, ww);
                    float dy = torus_delta_f(b->pos.y - st.world.player.pos.y, hh);
                    float d2 = dx * dx + dy * dy;
                    if (d2 < hitR2) { dead = true; break; }
                }
                if (dead) {
                    app_reset_world_for_mode(&st);
                }
            }

            if (st.gameMode == GAMEMODE_TERMINATE44) {
                const float killR = 1.4f;
                const float killR2 = killR * killR;
                const float ww = (float)st.world.width;
                const float hh = (float)st.world.height;
                for (size_t i = 0; i < st.world.boidCount; i++) {
                    Boid* b = &st.world.boids[i];
                    if (!b->alive) continue;
                    if (b->predator) continue;
                    float dx = torus_delta_f(b->pos.x - st.world.player.pos.x, ww);
                    float dy = torus_delta_f(b->pos.y - st.world.player.pos.y, hh);
                    float d2 = dx * dx + dy * dy;
                    if (d2 < killR2) {
                        b->alive = 0;
                        st.terminateKills++;
                    }
                }
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
