#include "app.h"

#include "boids.h"
#include "input.h"
#include "render.h"
#include "timeutil.h"
#include "update_winapi.h"

#include <stdio.h>

bool app_run(const AppConfig* cfg) {
    World w;
    if (!world_init(&w, cfg->width, cfg->height, (size_t)cfg->boidCount)) {
        fprintf(stderr, "world_init failed\n");
        return false;
    }

    Input in;
    input_init(&in);

    Renderer r;
    renderer_init(&r, cfg->width, cfg->height);

    UpdateWinApi upd;
    if (!update_winapi_init(&upd, (size_t)cfg->threadCount)) {
        fprintf(stderr, "update_winapi_init failed\n");
        world_destroy(&w);
        return false;
    }

    const double dt = 1.0 / (double)cfg->stepsPerSecond;

    const uint64_t startUs = time_now_us();
    const uint64_t maxRunUs = (cfg->runSeconds > 0) ? (uint64_t)cfg->runSeconds * 1000000ULL : 0ULL;

    double avgMs = 0.0;
    int frame = 0;

    while (true) {
        if (maxRunUs > 0) {
            uint64_t nowUs = time_now_us();
            if (nowUs - startUs >= maxRunUs) break;
        }

        InputState st = input_poll(&in);
        if (st.quit) break;

        world_apply_player_input(&w, &st, dt);

        uint64_t t0 = time_now_us();
        update_winapi_step(&upd, &w, dt);
        uint64_t t1 = time_now_us();

        double ms = (double)(t1 - t0) / 1000.0;
        frame++;
        avgMs += (ms - avgMs) / (double)frame;

        renderer_draw_world(&r, &w, avgMs, cfg->threadCount);
        time_sleep_us((uint64_t)(dt * 1000000.0));
    }

    update_winapi_destroy(&upd);
    renderer_shutdown(&r);
    world_destroy(&w);
    return true;
}
