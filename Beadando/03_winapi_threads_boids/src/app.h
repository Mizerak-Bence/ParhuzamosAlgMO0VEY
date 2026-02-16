#pragma once

#include <stdbool.h>

typedef struct AppConfig {
    int width;
    int height;
    int boidCount;
    int threadCount;
    int stepsPerSecond;
    int runSeconds;
} AppConfig;

bool app_run(const AppConfig* cfg);
