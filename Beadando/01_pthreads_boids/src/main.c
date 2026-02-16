#include "boids.h"
#include "update_pthreads.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <conio.h>
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
    int stepsPerSecond;
    int runSeconds;
} AppConfig;

static void print_usage(const char* exe) {
    printf("Usage: %s [--mode seq|pthread] [--threads N] [--boids N] [--width W] [--height H] [--seconds S]\n", exe);
    printf("Controls: WASD move player, Q quit\n");
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

static void clear_screen(void) {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H");
#endif
}

static bool poll_quit_and_input(InputState* outInput) {
    InputState st = {0};
    bool quit = false;

#ifdef _WIN32
    while (_kbhit()) {
        int c = _getch();
        if (c == 'q' || c == 'Q') quit = true;
        if (c == 'w' || c == 'W') st.up = true;
        if (c == 's' || c == 'S') st.down = true;
        if (c == 'a' || c == 'A') st.left = true;
        if (c == 'd' || c == 'D') st.right = true;
    }
#endif

    *outInput = st;
    return quit;
}

static void render_world(const World* w, double avgMs, RunMode mode, int threads, char* frameBuf) {
    clear_screen();
    printf("Boids=%zu | mode=%s | threads=%d | avg=%.3f ms/tick\n",
           w->boidCount,
           mode == RUNMODE_SEQ ? "seq" : "pthread",
           threads,
           avgMs);
    printf("Controls: WASD move player, Q quit\n\n");

    const int width = w->width;
    const int height = w->height;
    const size_t n = (size_t)width * (size_t)height;
    memset(frameBuf, ' ', n);

    for (size_t i = 0; i < w->boidCount; i++) {
        int x = (int)(w->boids[i].pos.x + 0.5f);
        int y = (int)(w->boids[i].pos.y + 0.5f);
        if (x >= 0 && x < width && y >= 0 && y < height) {
            frameBuf[(size_t)y * (size_t)width + (size_t)x] = 'o';
        }
    }

    int px = (int)(w->player.pos.x + 0.5f);
    int py = (int)(w->player.pos.y + 0.5f);
    if (px >= 0 && px < width && py >= 0 && py < height) {
        frameBuf[(size_t)py * (size_t)width + (size_t)px] = '@';
    }

    for (int y = 0; y < height; y++) {
        fwrite(&frameBuf[(size_t)y * (size_t)width], 1, (size_t)width, stdout);
        fputc('\n', stdout);
    }
}

int main(int argc, char** argv) {
    AppConfig cfg = {
        .width = 80,
        .height = 25,
        .boidCount = 200,
        .threadCount = 4,
        .mode = RUNMODE_PTHREAD,
        .stepsPerSecond = 30,
        .runSeconds = 0,
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
        if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) { cfg.runSeconds = parse_int(argv[++i], cfg.runSeconds); continue; }

        fprintf(stderr, "Unknown arg: %s\n", argv[i]);
        print_usage(argv[0]);
        return 2;
    }

    if (cfg.width <= 10 || cfg.height <= 10 || cfg.boidCount <= 0 || cfg.threadCount <= 0 || cfg.stepsPerSecond <= 0) {
        fprintf(stderr, "Invalid config. Use --help\n");
        return 2;
    }

    srand((unsigned)time_now_us());

    World world;
    if (!world_init(&world, cfg.width, cfg.height, (size_t)cfg.boidCount)) {
        fprintf(stderr, "world_init failed\n");
        return 1;
    }

    UpdatePthreads updater = {0};
    if (cfg.mode == RUNMODE_PTHREAD) {
        if (!update_pthreads_init(&updater, (size_t)cfg.threadCount)) {
            fprintf(stderr, "update_pthreads_init failed\n");
            world_destroy(&world);
            return 1;
        }
    }

    const double dt = 1.0 / (double)cfg.stepsPerSecond;
    const uint64_t startUs = time_now_us();
    const uint64_t maxRunUs = (cfg.runSeconds > 0) ? (uint64_t)cfg.runSeconds * 1000000ULL : 0ULL;

    char* frameBuf = (char*)malloc((size_t)cfg.width * (size_t)cfg.height);
    if (!frameBuf) {
        fprintf(stderr, "Out of memory\n");
        if (cfg.mode == RUNMODE_PTHREAD) update_pthreads_destroy(&updater);
        world_destroy(&world);
        return 1;
    }

    double avgMs = 0.0;
    int frameCount = 0;

    while (true) {
        if (maxRunUs > 0) {
            uint64_t nowUs = time_now_us();
            if (nowUs - startUs >= maxRunUs) break;
        }

        InputState in;
        if (poll_quit_and_input(&in)) break;

        world_apply_player_input(&world, &in, dt);

        const uint64_t t0 = time_now_us();
        if (cfg.mode == RUNMODE_SEQ) {
            world_step_range(&world, &world, 0, world.boidCount, dt);
            world_swap_buffers(&world);
        } else {
            update_pthreads_step(&updater, &world, dt);
        }
        const uint64_t t1 = time_now_us();

        const double ms = (double)(t1 - t0) / 1000.0;
        frameCount++;
        avgMs += (ms - avgMs) / (double)frameCount;

        render_world(&world, avgMs, cfg.mode, cfg.threadCount, frameBuf);
        time_sleep_us((uint64_t)(dt * 1000000.0));
    }

    free(frameBuf);

    if (cfg.mode == RUNMODE_PTHREAD) {
        update_pthreads_destroy(&updater);
    }
    world_destroy(&world);
    return 0;
}
