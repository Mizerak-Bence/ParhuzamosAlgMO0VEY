#include "life.h"
#include "render.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "timeutil.h"

static int parse_int(const char* s, int def) {
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s) return def;
    return (int)v;
}

int main(int argc, char** argv) {
    AppConfig cfg = {
        .width = 80,
        .height = 25,
        .threads = 4,
        .runSeconds = 0,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) cfg.width = parse_int(argv[++i], cfg.width);
        else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) cfg.height = parse_int(argv[++i], cfg.height);
        else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) cfg.threads = parse_int(argv[++i], cfg.threads);
        else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) cfg.runSeconds = parse_int(argv[++i], cfg.runSeconds);
        else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--width W] [--height H] [--threads N] [--seconds S]\n", argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            return 2;
        }
    }

    Life life;
    if (!life_init(&life, cfg.width, cfg.height)) {
        fprintf(stderr, "life_init failed\n");
        return 1;
    }

    Renderer r;
    renderer_init(&r, cfg.width, cfg.height);

    Input in;
    input_init(&in);

    int cursorX = cfg.width / 2;
    int cursorY = cfg.height / 2;
    bool running = true;

    const uint64_t startUs = time_now_us();
    const uint64_t maxRunUs = (cfg.runSeconds > 0) ? (uint64_t)cfg.runSeconds * 1000000ULL : 0ULL;

    life_seed_glider(&life, cursorX, cursorY);

    while (true) {
        if (maxRunUs > 0) {
            uint64_t nowUs = time_now_us();
            if (nowUs - startUs >= maxRunUs) break;
        }

        InputState st = input_poll(&in);
        if (st.quit) break;

        if (st.pauseToggle) running = !running;

        if (st.left && cursorX > 0) cursorX--;
        if (st.right && cursorX < cfg.width - 1) cursorX++;
        if (st.up && cursorY > 0) cursorY--;
        if (st.down && cursorY < cfg.height - 1) cursorY++;

        if (st.toggle) {
            life_toggle(&life, cursorX, cursorY);
        }

        double stepMs = 0.0;
        if (running) {
            stepMs = life_step_openmp(&life, cfg.threads);
        }

        renderer_draw(&r, &life, cursorX, cursorY, running, cfg.threads, stepMs);
    }

    renderer_shutdown(&r);
    life_destroy(&life);
    return 0;
}
