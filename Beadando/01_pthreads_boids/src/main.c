#include "boids.h"
#include "update_pthreads.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#ifdef _WIN32
    static LARGE_INTEGER freq;
    static int init = 0;
    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = 1;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart * 1000000ULL) / (uint64_t)(freq.QuadPart));
#else
    return 0;
#endif
}

static void time_sleep_us(uint64_t us) {
#ifdef _WIN32
    if (us == 0) return;
    DWORD ms = (DWORD)(us / 1000ULL);
    if (ms == 0) ms = 1;
    Sleep(ms);
#else
    (void)us;
#endif
}

#ifdef _WIN32

typedef struct AppState {
    AppConfig cfg;
    World world;
    UpdatePthreads updater;
    bool updaterInited;

    InputState input;
    bool quit;

    double avgMs;
    int avgCount;

    HDC backDC;
    HBITMAP backBmp;
    HGDIOBJ backOld;
    int backW;
    int backH;
} AppState;

static void input_set_key(InputState* in, WPARAM vk, bool down) {
    if (vk == 'W') in->up = down;
    if (vk == 'S') in->down = down;
    if (vk == 'A') in->left = down;
    if (vk == 'D') in->right = down;
}

static void backbuffer_destroy(AppState* s) {
    if (!s) return;
    if (s->backDC) {
        if (s->backOld) SelectObject(s->backDC, s->backOld);
        s->backOld = NULL;
    }
    if (s->backBmp) {
        DeleteObject(s->backBmp);
        s->backBmp = NULL;
    }
    if (s->backDC) {
        DeleteDC(s->backDC);
        s->backDC = NULL;
    }
    s->backW = 0;
    s->backH = 0;
}

static bool backbuffer_ensure(HWND hwnd, AppState* s, int w, int h) {
    if (w <= 0 || h <= 0) return false;
    if (s->backDC && s->backBmp && s->backW == w && s->backH == h) return true;

    backbuffer_destroy(s);

    HDC dc = GetDC(hwnd);
    if (!dc) return false;
    s->backDC = CreateCompatibleDC(dc);
    s->backBmp = CreateCompatibleBitmap(dc, w, h);
    ReleaseDC(hwnd, dc);
    if (!s->backDC || !s->backBmp) {
        backbuffer_destroy(s);
        return false;
    }
    s->backOld = SelectObject(s->backDC, s->backBmp);
    s->backW = w;
    s->backH = h;
    return true;
}

static void draw_world_gdi(AppState* s, HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;
    if (!backbuffer_ensure(hwnd, s, cw, ch)) return;

    HBRUSH bg = (HBRUSH)GetStockObject(BLACK_BRUSH);
    FillRect(s->backDC, &rc, bg);

    const int hudH = 26;
    const int drawW = cw;
    const int drawH = (ch - hudH) > 1 ? (ch - hudH) : 1;
    const float sx = (s->world.width > 0) ? ((float)drawW / (float)(s->world.width)) : 1.0f;
    const float sy = (s->world.height > 0) ? ((float)drawH / (float)(s->world.height)) : 1.0f;
    float scale = sx < sy ? sx : sy;
    if (scale < 1.0f) scale = 1.0f;

    SetBkMode(s->backDC, TRANSPARENT);
    SetTextColor(s->backDC, RGB(220, 220, 220));

    char hud[256];
    _snprintf_s(hud, sizeof(hud), _TRUNCATE,
                "Boids=%zu | mode=%s | threads=%d | avg=%.3f ms/tick | WASD move | Q/ESC quit",
                s->world.boidCount,
                s->cfg.mode == RUNMODE_SEQ ? "seq" : "pthread",
                s->cfg.threadCount,
                s->avgMs);
    TextOutA(s->backDC, 6, 6, hud, (int)strlen(hud));

    const float worldPixW = (float)s->world.width * scale;
    const float worldPixH = (float)s->world.height * scale;
    const float ox = 0.5f * ((float)drawW - worldPixW);
    const float oy = (float)hudH + 0.5f * ((float)drawH - worldPixH);

    HPEN penBoid = CreatePen(PS_SOLID, 1, RGB(200, 200, 255));
    HBRUSH brBoid = CreateSolidBrush(RGB(80, 80, 220));
    HPEN oldPen = (HPEN)SelectObject(s->backDC, penBoid);
    HBRUSH oldBrush = (HBRUSH)SelectObject(s->backDC, brBoid);

    float triSize = 6.0f;
    if (scale > 0.5f) triSize = 0.6f * scale;
    if (triSize < 4.0f) triSize = 4.0f;
    if (triSize > 14.0f) triSize = 14.0f;

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
        POINT pts[3];
        pts[0] = (POINT){(LONG)(hx + 0.5f), (LONG)(hy + 0.5f)};
        pts[1] = (POINT){(LONG)(lx + 0.5f), (LONG)(ly + 0.5f)};
        pts[2] = (POINT){(LONG)(rx + 0.5f), (LONG)(ry + 0.5f)};
        Polygon(s->backDC, pts, 3);
    }

    SelectObject(s->backDC, oldPen);
    SelectObject(s->backDC, oldBrush);
    DeleteObject(penBoid);
    DeleteObject(brBoid);

    HPEN penPlayer = CreatePen(PS_SOLID, 1, RGB(255, 180, 180));
    HBRUSH brPlayer = CreateSolidBrush(RGB(220, 60, 60));
    oldPen = (HPEN)SelectObject(s->backDC, penPlayer);
    oldBrush = (HBRUSH)SelectObject(s->backDC, brPlayer);

    float ppx = ox + s->world.player.pos.x * scale;
    float ppy = oy + s->world.player.pos.y * scale;
    int pr = (int)(triSize * 0.7f);
    Ellipse(s->backDC, (int)(ppx - pr), (int)(ppy - pr), (int)(ppx + pr), (int)(ppy + pr));

    SelectObject(s->backDC, oldPen);
    SelectObject(s->backDC, oldBrush);
    DeleteObject(penPlayer);
    DeleteObject(brPlayer);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppState* s = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            if (s) {
                input_set_key(&s->input, wParam, true);
                if (wParam == VK_ESCAPE || wParam == 'Q') {
                    s->quit = true;
                    PostQuitMessage(0);
                }
            }
            return 0;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            if (s) input_set_key(&s->input, wParam, false);
            return 0;
        }
        case WM_KILLFOCUS: {
            if (s) s->input = (InputState){0};
            return 0;
        }
        case WM_CLOSE: {
            if (s) s->quit = true;
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE: {
            if (s) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                backbuffer_ensure(hwnd, s, rc.right - rc.left, rc.bottom - rc.top);
            }
            return 0;
        }
        case WM_PAINT: {
            if (!s) break;
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            draw_world_gdi(s, hwnd);
            if (s->backDC && s->backBmp) {
                BitBlt(dc, 0, 0, s->backW, s->backH, s->backDC, 0, 0, SRCCOPY);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

#endif

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

    srand((unsigned)time_now_us());

#ifdef _WIN32
    AppState st = {0};
    st.cfg = cfg;

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

    HINSTANCE inst = GetModuleHandle(NULL);
    const char* cls = "BoidsPthreadsWindow";

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = cls;
    if (!RegisterClassA(&wc)) {
        fprintf(stderr, "RegisterClass failed\n");
        if (st.updaterInited) update_pthreads_destroy(&st.updater);
        world_destroy(&st.world);
        return 1;
    }

    const int scale = 10;
    int clientW = cfg.width * scale;
    int clientH = cfg.height * scale + 26;
    RECT wr = {0, 0, clientW, clientH};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExA(
        0, cls, "Boids (pthreads)", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, inst, &st);

    if (!hwnd) {
        fprintf(stderr, "CreateWindow failed\n");
        if (st.updaterInited) update_pthreads_destroy(&st.updater);
        world_destroy(&st.world);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    const double simDt = 1.0 / 120.0;
    double acc = 0.0;
    uint64_t lastUs = time_now_us();

    MSG msg;
    while (!st.quit) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { st.quit = true; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (st.quit) break;

        uint64_t nowUs = time_now_us();
        double frameDt = (double)(nowUs - lastUs) / 1000000.0;
        lastUs = nowUs;
        if (frameDt > 0.05) frameDt = 0.05;
        acc += frameDt;

        while (acc >= simDt) {
            world_apply_player_input(&st.world, &st.input, simDt);

            const uint64_t t0 = time_now_us();
            if (cfg.mode == RUNMODE_SEQ) {
                world_step_range(&st.world, &st.world, 0, st.world.boidCount, simDt);
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

        InvalidateRect(hwnd, NULL, FALSE);
        time_sleep_us(1000);
    }

    backbuffer_destroy(&st);
    if (st.updaterInited) update_pthreads_destroy(&st.updater);
    world_destroy(&st.world);
    return 0;
#else
    (void)argc;
    (void)argv;
    fprintf(stderr, "This program requires Windows WinAPI for visualization.\n");
    return 1;
#endif
}
