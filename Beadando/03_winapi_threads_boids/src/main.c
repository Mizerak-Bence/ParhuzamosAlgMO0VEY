#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char* exe) {
    printf("Usage: %s [--threads N] [--boids N] [--width W] [--height H]\n", exe);
}

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
