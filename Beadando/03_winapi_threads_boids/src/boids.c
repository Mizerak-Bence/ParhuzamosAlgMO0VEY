#include "boids.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static float frand01(void) { return (float)rand() / (float)RAND_MAX; }

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

static Vec2 wrap_pos(const World* w, Vec2 p) {
    float ww = (float)w->width;
    float hh = (float)w->height;
    p.x = wrapf(p.x, ww);
    p.y = wrapf(p.y, hh);
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
    float maxLen2 = maxLen * maxLen;

    if (l2 <= maxLen2) return v;

    return v_mul(v, maxLen / sqrtf(l2));
}

static Vec2 steer_towards(Vec2 desiredVel, Vec2 currentVel, float maxForce) {
    return v_limit(v_sub(desiredVel, currentVel), maxForce);
}

bool world_init(World* w, int width, int height, size_t boidCount) {
    memset(w, 0, sizeof(*w));
    w->width = width;
    w->height = height;
    w->groupCount = 1;
    w->boidCount = boidCount;

    w->boids = (Boid*)calloc(boidCount, sizeof(Boid));
    w->boidsNext = (Boid*)calloc(boidCount, sizeof(Boid));
    if (!w->boids || !w->boidsNext) {
        free(w->boids);
        free(w->boidsNext);
        return false;
    }
    {
        int groupCount = (int)(boidCount / 60);
        Vec2 centers[16];
        Vec2 dirs[16];
        const float baseSpeed = 14.0f;

        if (groupCount < 6) groupCount = 6;
        if (groupCount > 16) groupCount = 16;
        w->groupCount = groupCount;

        for (int g = 0; g < groupCount; g++) {
            Vec2 c = {0, 0};
            const float minD2 = (float)(width * width + height * height) * 0.02f;

            for (int tries = 0; tries < 60; tries++) {
                bool ok = true;

                c = (Vec2){
                    frand_range(0.10f * (float)width, 0.90f * (float)width),
                    frand_range(0.10f * (float)height, 0.90f * (float)height),
                };

                for (int k = 0; k < g; k++) {
                    float dx = torus_delta(c.x - centers[k].x, (float)width);
                    float dy = torus_delta(c.y - centers[k].y, (float)height);
                    float d2 = dx * dx + dy * dy;

                    if (d2 < minD2) {
                        ok = false;
                        break;
                    }
                }

                if (ok) break;
            }

            centers[g] = c;

            {
                float angle = frand_range(0.0f, 6.2831853f);
                dirs[g] = (Vec2){cosf(angle), sinf(angle)};
            }
        }

        for (size_t i = 0; i < boidCount; i++) {
            int g = (int)(i % (size_t)groupCount);
            float spreadX = (float)width * 0.07f;
            float spreadY = (float)height * 0.10f;
            Vec2 jitterDir;
            float speed;

            w->boids[i].pos = wrap_pos(w, v_add(centers[g], (Vec2){frand_signed() * spreadX, frand_signed() * spreadY}));
            w->boids[i].group = (unsigned char)g;
            w->boids[i].alive = 1;
            w->boids[i].predator = 0;

            speed = baseSpeed + frand_signed() * 5.0f;
            jitterDir = v_norm((Vec2){dirs[g].x + frand_signed() * 0.30f, dirs[g].y + frand_signed() * 0.30f});
            w->boids[i].vel = v_mul(jitterDir, speed);
        }
    }

    w->player.pos = (Vec2){(float)(width / 2), (float)(height / 2)};
    w->player.speed = 25.0f;
    return true;
}

void world_destroy(World* w) {
    if (!w) return;
    free(w->boids);
    free(w->boidsNext);
    memset(w, 0, sizeof(*w));
}

void world_apply_player_input(World* w, const struct InputState* in, double dt) {
    Vec2 d = {0, 0};
    if (in->up) d.y -= 1;
    if (in->down) d.y += 1;
    if (in->left) d.x -= 1;
    if (in->right) d.x += 1;

    if (d.x != 0 || d.y != 0) {
        d = v_norm(d);
        w->player.pos = v_add(w->player.pos, v_mul(d, (float)(w->player.speed * dt)));
        w->player.pos = wrap_pos(w, w->player.pos);
    }
}

double world_step_seq(World* w, double dt) {
    uint64_t t0;
    uint64_t t1;
    Boid* tmp;

#ifdef _WIN32
    static LARGE_INTEGER freq;
    static int init = 0;
    LARGE_INTEGER now0;
    LARGE_INTEGER now1;

    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = 1;
    }

    QueryPerformanceCounter(&now0);
    t0 = (uint64_t)((now0.QuadPart * 1000000ULL) / (uint64_t)freq.QuadPart);
#else
    t0 = 0;
#endif

    world_step_range(w, w, 0, w->boidCount, dt);
    tmp = w->boids;
    w->boids = w->boidsNext;
    w->boidsNext = tmp;

#ifdef _WIN32
    QueryPerformanceCounter(&now1);
    t1 = (uint64_t)((now1.QuadPart * 1000000ULL) / (uint64_t)freq.QuadPart);
#else
    t1 = t0;
#endif

    return (double)(t1 - t0) / 1000.0;
}

void world_step_range(const World* r, World* w, size_t begin, size_t end, double dt) {
    const float neighborRadius = 6.5f;
    const float desiredSeparation = 2.2f;
    const float desiredSeparation2 = desiredSeparation * desiredSeparation;
    const float maxSpeed = 30.0f;
    const float maxForce = 25.0f;
    const float wCohesion = 0.45f;
    const float wAlignment = 0.85f;
    const float wSeparation = 1.45f;
    const float wAvoidPlayer = 1.50f;
    const float neighborRadius2 = neighborRadius * neighborRadius;
    const float worldW = (float)r->width;
    const float worldH = (float)r->height;
    const float eps2 = 1e-8f;
    const float predMaxSpeed = 44.0f;
    const float predMaxForce = 50.0f;
    const float predSepRadius = 4.0f;
    const float predSepRadius2 = predSepRadius * predSepRadius;
    const float wPredChase = 2.00f;
    const float wPredSep = 1.10f;
    const float predAvoidRadius = 12.0f;
    const float predAvoidRadius2 = predAvoidRadius * predAvoidRadius;
    const float wAvoidPred = 2.20f;

    for (size_t i = begin; i < end; i++) {
        Boid b = r->boids[i];

        if (!b.alive) {
            w->boidsNext[i] = b;
            continue;
        }

        if (b.predator) {
            Vec2 sumSep = {0, 0};

            for (size_t j = 0; j < r->boidCount; j++) {
                float dx;
                float dy;
                float d2;

                if (i == j) continue;
                if (!r->boids[j].alive) continue;

                dx = torus_delta(r->boids[j].pos.x - b.pos.x, worldW);
                dy = torus_delta(r->boids[j].pos.y - b.pos.y, worldH);
                d2 = dx * dx + dy * dy;

                if (d2 < predSepRadius2 && d2 > 1e-6f) {
                    Vec2 away = v_mul(v_norm((Vec2){dx, dy}), -1.0f);
                    sumSep = v_add(sumSep, v_div(away, sqrtf(d2)));
                }
            }

            {
                float pdx = torus_delta(r->player.pos.x - b.pos.x, worldW);
                float pdy = torus_delta(r->player.pos.y - b.pos.y, worldH);
                Vec2 toPlayer = (Vec2){pdx, pdy};
                Vec2 accel = {0, 0};

                if (v_len2(sumSep) > eps2) {
                    Vec2 desiredS = v_mul(v_norm(sumSep), predMaxSpeed);
                    Vec2 sep = steer_towards(desiredS, b.vel, predMaxForce);
                    accel = v_add(accel, v_mul(sep, wPredSep));
                }
                if (v_len2(toPlayer) > eps2) {
                    Vec2 desiredC = v_mul(v_norm(toPlayer), predMaxSpeed);
                    Vec2 chase = steer_towards(desiredC, b.vel, predMaxForce);
                    accel = v_add(accel, v_mul(chase, wPredChase));
                }

                b.vel = v_add(b.vel, v_mul(accel, (float)dt));
                b.vel = v_limit(b.vel, predMaxSpeed);
                b.pos = wrap_pos(r, v_add(b.pos, v_mul(b.vel, (float)dt)));
                w->boidsNext[i] = b;
                continue;
            }
        }

        {
            const unsigned char groupI = b.group;
            Vec2 sumPos = {0, 0};
            Vec2 sumVel = {0, 0};
            Vec2 sumSep = {0, 0};
            Vec2 sumPred = {0, 0};
            int neighbors = 0;

            for (size_t j = 0; j < r->boidCount; j++) {
                float dx;
                float dy;
                float d2;

                if (i == j) continue;
                if (!r->boids[j].alive) continue;

                dx = torus_delta(r->boids[j].pos.x - b.pos.x, worldW);
                dy = torus_delta(r->boids[j].pos.y - b.pos.y, worldH);
                d2 = dx * dx + dy * dy;

                if (d2 < desiredSeparation2 && d2 > 1e-6f) {
                    Vec2 away = v_mul(v_norm((Vec2){dx, dy}), -1.0f);
                    sumSep = v_add(sumSep, v_div(away, sqrtf(d2)));
                }

                if (r->boids[j].predator) {
                    if (d2 < predAvoidRadius2 && d2 > 1e-6f) {
                        Vec2 away = v_mul(v_norm((Vec2){dx, dy}), -1.0f);
                        sumPred = v_add(sumPred, v_div(away, sqrtf(d2)));
                    }
                    continue;
                }

                if (r->boids[j].group != groupI) continue;

                if (d2 < neighborRadius2) {
                    neighbors++;
                    sumPos = v_add(sumPos, v_add(b.pos, (Vec2){dx, dy}));
                    sumVel = v_add(sumVel, r->boids[j].vel);
                }
            }

            {
                Vec2 accel = {0, 0};

                if (neighbors > 0) {
                    Vec2 center = v_mul(sumPos, 1.0f / (float)neighbors);
                    Vec2 avgVel = v_mul(sumVel, 1.0f / (float)neighbors);

                    if (v_len2(v_sub(center, b.pos)) > eps2) {
                        Vec2 desiredC = v_mul(v_norm(v_sub(center, b.pos)), maxSpeed);
                        accel = v_add(accel, v_mul(steer_towards(desiredC, b.vel, maxForce), wCohesion));
                    }
                    if (v_len2(avgVel) > eps2) {
                        Vec2 desiredA = v_mul(v_norm(avgVel), maxSpeed);
                        accel = v_add(accel, v_mul(steer_towards(desiredA, b.vel, maxForce), wAlignment));
                    }
                    if (v_len2(sumSep) > eps2) {
                        Vec2 desiredS = v_mul(v_norm(sumSep), maxSpeed);
                        accel = v_add(accel, v_mul(steer_towards(desiredS, b.vel, maxForce), wSeparation));
                    }
                } else if (v_len2(sumSep) > eps2) {
                    Vec2 desiredS = v_mul(v_norm(sumSep), maxSpeed);
                    accel = v_add(accel, v_mul(steer_towards(desiredS, b.vel, maxForce), wSeparation));
                }

                if (v_len2(sumPred) > eps2) {
                    Vec2 desiredP = v_mul(v_norm(sumPred), maxSpeed);
                    accel = v_add(accel, v_mul(steer_towards(desiredP, b.vel, maxForce), wAvoidPred));
                }

                {
                    float pdx = torus_delta(r->player.pos.x - b.pos.x, worldW);
                    float pdy = torus_delta(r->player.pos.y - b.pos.y, worldH);
                    float dp2 = pdx * pdx + pdy * pdy;

                    if (dp2 < 100.0f && dp2 > 1e-6f) {
                        Vec2 desired = v_mul(v_norm((Vec2){-pdx, -pdy}), maxSpeed);
                        accel = v_add(accel, v_mul(steer_towards(desired, b.vel, maxForce), wAvoidPlayer));
                    }
                }

                b.vel = v_limit(v_add(b.vel, v_mul(accel, (float)dt)), maxSpeed);
                b.pos = wrap_pos(r, v_add(b.pos, v_mul(b.vel, (float)dt)));
                w->boidsNext[i] = b;
            }
        }
    }
}
