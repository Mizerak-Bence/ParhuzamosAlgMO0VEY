#include "boids.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <time.h>
#endif

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

static float frand01(void) {
    return (float)rand() / (float)RAND_MAX;
}

static float frand_range(float a, float b) {
    return a + (b - a) * frand01();
}

static float frand_signed(void) {
    return frand01() * 2.0f - 1.0f;
}

static Vec2 v_add(Vec2 a, Vec2 b) { return (Vec2){a.x + b.x, a.y + b.y}; }
static Vec2 v_sub(Vec2 a, Vec2 b) { return (Vec2){a.x - b.x, a.y - b.y}; }
static Vec2 v_mul(Vec2 a, float k) { return (Vec2){a.x * k, a.y * k}; }
static Vec2 v_div(Vec2 a, float k) { return (Vec2){a.x / k, a.y / k}; }
static float v_len2(Vec2 a) { return a.x * a.x + a.y * a.y; }
static float v_len(Vec2 a) { return sqrtf(v_len2(a)); }

static Vec2 v_norm(Vec2 a) {
    float l = v_len(a);
    if (l < 1e-6f) return (Vec2){0, 0};
    return v_mul(a, 1.0f / l);
}

static float wrapf(float x, float size) {
    if (size <= 0.0f) return x;
    while (x < 0.0f) x += size;
    while (x >= size) x -= size;
    return x;
}

static Vec2 wrap_pos(const World* world, Vec2 p) {
    p.x = wrapf(p.x, (float)world->width);
    p.y = wrapf(p.y, (float)world->height);
    return p;
}

static float torus_delta(float d, float size) {
    if (size <= 0.0f) return d;
    if (d > 0.5f * size) d -= size;
    else if (d < -0.5f * size) d += size;
    return d;
}

static Vec2 v_limit(Vec2 v, float maxLen) {
    float l2 = v_len2(v);
    if (l2 <= maxLen * maxLen) return v;
    if (l2 < 1e-8f) return (Vec2){0, 0};
    return v_mul(v, maxLen / sqrtf(l2));
}

static Vec2 steer_towards(Vec2 desiredVel, Vec2 currentVel, float maxForce) {
    return v_limit(v_sub(desiredVel, currentVel), maxForce);
}

bool world_init(World* world, int width, int height, size_t boidCount) {
    Vec2 centers[16];
    Vec2 dirs[16];
    int groupCount;
    const float baseSpeed = 14.0f;

    memset(world, 0, sizeof(*world));
    world->width = width;
    world->height = height;
    world->boidCount = boidCount;

    world->boids = (Boid*)calloc(boidCount, sizeof(Boid));
    world->boidsNext = (Boid*)calloc(boidCount, sizeof(Boid));
    if (!world->boids || !world->boidsNext) {
        free(world->boids);
        free(world->boidsNext);
        memset(world, 0, sizeof(*world));
        return false;
    }

    groupCount = (int)(boidCount / 60);
    if (groupCount < 6) groupCount = 6;
    if (groupCount > 16) groupCount = 16;
    world->groupCount = groupCount;

    for (int g = 0; g < groupCount; g++) {
        Vec2 center = {0, 0};
        const float minDist2 = (float)(width * width + height * height) * 0.02f;

        for (int tries = 0; tries < 60; tries++) {
            center = (Vec2){
                frand_range(0.10f * (float)width, 0.90f * (float)width),
                frand_range(0.10f * (float)height, 0.90f * (float)height),
            };

            bool separated = true;
            for (int k = 0; k < g; k++) {
                float dx = torus_delta(center.x - centers[k].x, (float)width);
                float dy = torus_delta(center.y - centers[k].y, (float)height);
                float d2 = dx * dx + dy * dy;
                if (d2 < minDist2) {
                    separated = false;
                    break;
                }
            }
            if (separated) break;
        }

        centers[g] = center;

        {
            float angle = frand_range(0.0f, 6.2831853f);
            dirs[g] = (Vec2){cosf(angle), sinf(angle)};
        }
    }

    for (size_t i = 0; i < boidCount; i++) {
        int group = (int)(i % (size_t)groupCount);
        float spreadX = (float)width * 0.07f;
        float spreadY = (float)height * 0.10f;
        Vec2 pos = v_add(centers[group], (Vec2){frand_signed() * spreadX, frand_signed() * spreadY});
        Vec2 dir = v_norm((Vec2){dirs[group].x + frand_signed() * 0.30f, dirs[group].y + frand_signed() * 0.30f});
        float speed = baseSpeed + frand_signed() * 5.0f;

        world->boids[i].pos = wrap_pos(world, pos);
        world->boids[i].vel = v_mul(dir, speed);
        world->boids[i].group = (unsigned char)group;
        world->boids[i].alive = 1;
        world->boids[i].predator = 0;
    }

    world->player.pos = (Vec2){(float)(width / 2), (float)(height / 2)};
    world->player.speed = 25.0f;
    return true;
}

void world_destroy(World* world) {
    if (!world) return;
    free(world->boids);
    free(world->boidsNext);
    memset(world, 0, sizeof(*world));
}

void world_apply_player_input(World* world, const InputState* input, double dt) {
    Vec2 delta = {0, 0};

    if (input->up) delta.y -= 1.0f;
    if (input->down) delta.y += 1.0f;
    if (input->left) delta.x -= 1.0f;
    if (input->right) delta.x += 1.0f;

    if (delta.x != 0.0f || delta.y != 0.0f) {
        delta = v_norm(delta);
        world->player.pos = v_add(world->player.pos, v_mul(delta, (float)(world->player.speed * dt)));
        world->player.pos = wrap_pos(world, world->player.pos);
    }
}

static void world_step_range(const World* worldRead, World* worldWrite, size_t begin, size_t end, double dt) {
    const float neighborRadius = 6.5f;
    const float desiredSeparation = 2.2f;
    const float desiredSeparation2 = desiredSeparation * desiredSeparation;
    const float neighborRadius2 = neighborRadius * neighborRadius;

    const float maxSpeed = 30.0f;
    const float maxForce = 25.0f;

    const float weightCohesion = 0.45f;
    const float weightAlignment = 0.85f;
    const float weightSeparation = 1.45f;
    const float weightAvoidPlayer = 1.50f;

    const float worldW = (float)worldRead->width;
    const float worldH = (float)worldRead->height;
    const float eps2 = 1e-8f;

    const float predMaxSpeed = 44.0f;
    const float predMaxForce = 50.0f;
    const float predSepRadius = 4.0f;
    const float predSepRadius2 = predSepRadius * predSepRadius;
    const float weightPredChase = 2.00f;
    const float weightPredSep = 1.10f;
    const float predAvoidRadius = 12.0f;
    const float predAvoidRadius2 = predAvoidRadius * predAvoidRadius;
    const float weightAvoidPred = 2.20f;

    for (size_t i = begin; i < end; i++) {
        Boid boid = worldRead->boids[i];

        if (!boid.alive) {
            worldWrite->boidsNext[i] = boid;
            continue;
        }

        if (boid.predator) {
            Vec2 sumSep = {0, 0};

            for (size_t j = 0; j < worldRead->boidCount; j++) {
                float dx;
                float dy;
                float d2;

                if (i == j) continue;
                if (!worldRead->boids[j].alive) continue;

                dx = torus_delta(worldRead->boids[j].pos.x - boid.pos.x, worldW);
                dy = torus_delta(worldRead->boids[j].pos.y - boid.pos.y, worldH);
                d2 = dx * dx + dy * dy;

                if (d2 < predSepRadius2 && d2 > 1e-6f) {
                    Vec2 away = v_mul(v_norm((Vec2){dx, dy}), -1.0f);
                    sumSep = v_add(sumSep, v_div(away, sqrtf(d2)));
                }
            }

            {
                float pdx = torus_delta(worldRead->player.pos.x - boid.pos.x, worldW);
                float pdy = torus_delta(worldRead->player.pos.y - boid.pos.y, worldH);
                Vec2 toPlayer = (Vec2){pdx, pdy};
                Vec2 accel = {0, 0};

                if (v_len2(sumSep) > eps2) {
                    Vec2 desiredS = v_mul(v_norm(sumSep), predMaxSpeed);
                    Vec2 sep = steer_towards(desiredS, boid.vel, predMaxForce);
                    accel = v_add(accel, v_mul(sep, weightPredSep));
                }
                if (v_len2(toPlayer) > eps2) {
                    Vec2 desiredC = v_mul(v_norm(toPlayer), predMaxSpeed);
                    Vec2 chase = steer_towards(desiredC, boid.vel, predMaxForce);
                    accel = v_add(accel, v_mul(chase, weightPredChase));
                }

                boid.vel = v_add(boid.vel, v_mul(accel, (float)dt));
                boid.vel = v_limit(boid.vel, predMaxSpeed);
                boid.pos = wrap_pos(worldRead, v_add(boid.pos, v_mul(boid.vel, (float)dt)));
            }

            worldWrite->boidsNext[i] = boid;
            continue;
        }

        {
            const unsigned char group = boid.group;
            Vec2 sumPos = {0, 0};
            Vec2 sumVel = {0, 0};
            Vec2 sumSep = {0, 0};
            Vec2 sumPred = {0, 0};
            int neighbors = 0;

            for (size_t j = 0; j < worldRead->boidCount; j++) {
                float dx;
                float dy;
                float d2;

                if (i == j) continue;
                if (!worldRead->boids[j].alive) continue;

                dx = torus_delta(worldRead->boids[j].pos.x - boid.pos.x, worldW);
                dy = torus_delta(worldRead->boids[j].pos.y - boid.pos.y, worldH);
                d2 = dx * dx + dy * dy;

                if (d2 < desiredSeparation2 && d2 > 1e-6f) {
                    Vec2 away = v_mul(v_norm((Vec2){dx, dy}), -1.0f);
                    sumSep = v_add(sumSep, v_div(away, sqrtf(d2)));
                }

                if (worldRead->boids[j].predator) {
                    if (d2 < predAvoidRadius2 && d2 > 1e-6f) {
                        Vec2 away = v_mul(v_norm((Vec2){dx, dy}), -1.0f);
                        sumPred = v_add(sumPred, v_div(away, sqrtf(d2)));
                    }
                    continue;
                }

                if (worldRead->boids[j].group != group) continue;

                if (d2 < neighborRadius2) {
                    neighbors++;
                    sumPos = v_add(sumPos, v_add(boid.pos, (Vec2){dx, dy}));
                    sumVel = v_add(sumVel, worldRead->boids[j].vel);
                }
            }

            {
                Vec2 accel = {0, 0};

                if (neighbors > 0) {
                    Vec2 center = v_mul(sumPos, 1.0f / (float)neighbors);
                    Vec2 avgVel = v_mul(sumVel, 1.0f / (float)neighbors);
                    Vec2 toCenter = v_sub(center, boid.pos);

                    if (v_len2(toCenter) > eps2) {
                        Vec2 desiredC = v_mul(v_norm(toCenter), maxSpeed);
                        accel = v_add(accel, v_mul(steer_towards(desiredC, boid.vel, maxForce), weightCohesion));
                    }
                    if (v_len2(avgVel) > eps2) {
                        Vec2 desiredA = v_mul(v_norm(avgVel), maxSpeed);
                        accel = v_add(accel, v_mul(steer_towards(desiredA, boid.vel, maxForce), weightAlignment));
                    }
                }

                if (v_len2(sumSep) > eps2) {
                    Vec2 desiredS = v_mul(v_norm(sumSep), maxSpeed);
                    accel = v_add(accel, v_mul(steer_towards(desiredS, boid.vel, maxForce), weightSeparation));
                }

                if (v_len2(sumPred) > eps2) {
                    Vec2 desiredPred = v_mul(v_norm(sumPred), maxSpeed);
                    accel = v_add(accel, v_mul(steer_towards(desiredPred, boid.vel, maxForce), weightAvoidPred));
                }

                {
                    float pdx = torus_delta(worldRead->player.pos.x - boid.pos.x, worldW);
                    float pdy = torus_delta(worldRead->player.pos.y - boid.pos.y, worldH);
                    float playerDist2 = pdx * pdx + pdy * pdy;
                    const float playerAvoidRadius = 10.0f;

                    if (playerDist2 < playerAvoidRadius * playerAvoidRadius && playerDist2 > 1e-6f) {
                        Vec2 desiredP = v_mul(v_norm((Vec2){-pdx, -pdy}), maxSpeed);
                        accel = v_add(accel, v_mul(steer_towards(desiredP, boid.vel, maxForce), weightAvoidPlayer));
                    }
                }

                boid.vel = v_add(boid.vel, v_mul(accel, (float)dt));
                boid.vel = v_limit(boid.vel, maxSpeed);
                boid.pos = wrap_pos(worldRead, v_add(boid.pos, v_mul(boid.vel, (float)dt)));
            }
        }

        worldWrite->boidsNext[i] = boid;
    }
}

static void world_swap_buffers(World* world) {
    Boid* tmp = world->boids;
    world->boids = world->boidsNext;
    world->boidsNext = tmp;
}

double world_step_seq(World* world, double dt) {
    uint64_t startUs = time_now_us();

    world_step_range(world, world, 0, world->boidCount, dt);
    world_swap_buffers(world);

    return (double)(time_now_us() - startUs) / 1000.0;
}

double world_step_openmp(World* world, int threads, double dt) {
    uint64_t startUs = time_now_us();

#ifdef _OPENMP
    omp_set_num_threads(threads);

#pragma omp parallel
    {
        int threadIndex = omp_get_thread_num();
        int threadCount = omp_get_num_threads();
        size_t begin = world->boidCount * (size_t)threadIndex / (size_t)threadCount;
        size_t end = world->boidCount * (size_t)(threadIndex + 1) / (size_t)threadCount;
        world_step_range(world, world, begin, end, dt);
    }
#else
    (void)threads;
    world_step_range(world, world, 0, world->boidCount, dt);
#endif

    world_swap_buffers(world);
    return (double)(time_now_us() - startUs) / 1000.0;
}