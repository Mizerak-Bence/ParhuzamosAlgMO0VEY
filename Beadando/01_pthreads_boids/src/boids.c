#include "boids.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

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

static Vec2 wrap_pos(const World* w, Vec2 p) {
    float ww = (float)w->width;
    float hh = (float)w->height;
    p.x = wrapf(p.x, ww);
    p.y = wrapf(p.y, hh);
    return p;
}

static float torus_delta(float d, float size) {
    if (size <= 0.0f) return d;
    float h = 0.5f * size;
    if (d > h) d -= size;
    else if (d < -h) d += size;
    return d;
}

static Vec2 v_limit(Vec2 v, float maxLen) {
    float l2 = v_len2(v);
    if (l2 <= maxLen * maxLen) return v;
    float l = sqrtf(l2);
    return v_mul(v, maxLen / l);
}

static Vec2 steer_towards(Vec2 desiredVel, Vec2 currentVel, float maxForce) {
    Vec2 steer = v_sub(desiredVel, currentVel);
    return v_limit(steer, maxForce);
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

    int groupCount = (int)(boidCount / 60);
    if (groupCount < 6) groupCount = 6;
    if (groupCount > 16) groupCount = 16;
    w->groupCount = groupCount;

    Vec2 centers[16];
    Vec2 dirs[16];

    for (int g = 0; g < groupCount; g++) {
        /* choose separated group centers so we don't get one huge blob */
        Vec2 c = {0, 0};
        const float minD2 = (float)(width * width + height * height) * 0.02f;
        for (int tries = 0; tries < 60; tries++) {
            c = (Vec2){
                frand_range(0.10f * (float)width, 0.90f * (float)width),
                frand_range(0.10f * (float)height, 0.90f * (float)height),
            };
            bool ok = true;
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

        float a = frand_range(0.0f, 6.2831853f);
        dirs[g] = (Vec2){cosf(a), sinf(a)};
    }

    const float baseSpeed = 14.0f;

    for (size_t i = 0; i < boidCount; i++) {
        /* balanced: round-robin groups */
        int g = (int)(i % (size_t)groupCount);
        float sizeFactor = 1.0f;

        float spreadX = (float)width * (0.03f + 0.04f * sizeFactor);
        float spreadY = (float)height * (0.04f + 0.06f * sizeFactor);
        Vec2 p = v_add(centers[g], (Vec2){frand_signed() * spreadX, frand_signed() * spreadY});
        p = wrap_pos(w, p);
        w->boids[i].pos = p;
        w->boids[i].group = (unsigned char)g;
        w->boids[i].alive = 1;
        w->boids[i].predator = 0;

        float sp = baseSpeed + frand_signed() * (3.0f + 2.0f * sizeFactor);
        Vec2 jitterDir = v_norm((Vec2){dirs[g].x + frand_signed() * 0.30f, dirs[g].y + frand_signed() * 0.30f});
        w->boids[i].vel = v_mul(jitterDir, sp);
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

void world_apply_player_input(World* w, const InputState* in, double dt) {
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
                if (i == j) continue;
                if (!r->boids[j].alive) continue;

                float dx = torus_delta(r->boids[j].pos.x - b.pos.x, worldW);
                float dy = torus_delta(r->boids[j].pos.y - b.pos.y, worldH);
                float d2 = dx * dx + dy * dy;
                if (d2 < predSepRadius2 && d2 > 1e-6f) {
                    Vec2 diff = (Vec2){dx, dy};
                    Vec2 away = v_mul(v_norm(diff), -1.0f);
                    sumSep = v_add(sumSep, v_div(away, sqrtf(d2)));
                }
            }

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
            b.pos = v_add(b.pos, v_mul(b.vel, (float)dt));
            b.pos = wrap_pos(r, b.pos);
            w->boidsNext[i] = b;
            continue;
        }

        const unsigned char groupI = b.group;

        Vec2 sumPos = {0, 0};
        Vec2 sumVel = {0, 0};
        Vec2 sumSep = {0, 0};
        Vec2 sumPred = {0, 0};
        int neighbors = 0;

        for (size_t j = 0; j < r->boidCount; j++) {
            if (i == j) continue;
            if (!r->boids[j].alive) continue;

            float dx = torus_delta(r->boids[j].pos.x - b.pos.x, worldW);
            float dy = torus_delta(r->boids[j].pos.y - b.pos.y, worldH);
            float d2 = dx * dx + dy * dy;

            /* separation against everyone */
            if (d2 < desiredSeparation2 && d2 > 1e-6f) {
                Vec2 diff = (Vec2){dx, dy};
                Vec2 away = v_mul(v_norm(diff), -1.0f);
                sumSep = v_add(sumSep, v_div(away, sqrtf(d2)));
            }

            /* avoid predators */
            if (r->boids[j].predator) {
                if (d2 < predAvoidRadius2 && d2 > 1e-6f) {
                    Vec2 diff = (Vec2){dx, dy};
                    Vec2 away = v_mul(v_norm(diff), -1.0f);
                    sumPred = v_add(sumPred, v_div(away, sqrtf(d2)));
                }
                continue;
            }

            /* cohesion/alignment only within the same group */
            if (r->boids[j].group != groupI) continue;
            if (d2 < neighborRadius2) {
                neighbors++;
                Vec2 localPos = v_add(b.pos, (Vec2){dx, dy});
                sumPos = v_add(sumPos, localPos);
                sumVel = v_add(sumVel, r->boids[j].vel);
            }
        }

        Vec2 accel = {0, 0};

        if (neighbors > 0) {
            Vec2 center = v_mul(sumPos, 1.0f / (float)neighbors);
            Vec2 avgVel = v_mul(sumVel, 1.0f / (float)neighbors);

            Vec2 coh = (Vec2){0, 0};
            Vec2 ali = (Vec2){0, 0};
            Vec2 sep = (Vec2){0, 0};

            Vec2 toCenter = v_sub(center, b.pos);
            if (v_len2(toCenter) > eps2) {
                Vec2 desiredC = v_mul(v_norm(toCenter), maxSpeed);
                coh = steer_towards(desiredC, b.vel, maxForce);
            }
            if (v_len2(avgVel) > eps2) {
                Vec2 desiredA = v_mul(v_norm(avgVel), maxSpeed);
                ali = steer_towards(desiredA, b.vel, maxForce);
            }
            if (v_len2(sumSep) > eps2) {
                Vec2 desiredS = v_mul(v_norm(sumSep), maxSpeed);
                sep = steer_towards(desiredS, b.vel, maxForce);
            }

            accel = v_add(accel, v_mul(coh, wCohesion));
            accel = v_add(accel, v_mul(ali, wAlignment));
            accel = v_add(accel, v_mul(sep, wSeparation));
        } else {
            /* still apply separation even if no same-group neighbors */
            if (v_len2(sumSep) > eps2) {
                Vec2 desiredS = v_mul(v_norm(sumSep), maxSpeed);
                Vec2 sep = steer_towards(desiredS, b.vel, maxForce);
                accel = v_add(accel, v_mul(sep, wSeparation));
            }
        }

        if (v_len2(sumPred) > eps2) {
            Vec2 desiredP = v_mul(v_norm(sumPred), maxSpeed);
            Vec2 ap = steer_towards(desiredP, b.vel, maxForce);
            accel = v_add(accel, v_mul(ap, wAvoidPred));
        }

        float pdx = torus_delta(r->player.pos.x - b.pos.x, worldW);
        float pdy = torus_delta(r->player.pos.y - b.pos.y, worldH);
        Vec2 toPlayer = (Vec2){pdx, pdy};
        float dp2 = pdx * pdx + pdy * pdy;

        const float playerAvoidRadius = 10.0f;
        if (dp2 < playerAvoidRadius * playerAvoidRadius && dp2 > 1e-6f) {
            Vec2 desired = v_mul(v_norm(v_mul(toPlayer, -1.0f)), maxSpeed);
            Vec2 avoid = steer_towards(desired, b.vel, maxForce);
            accel = v_add(accel, v_mul(avoid, wAvoidPlayer));
        }

        b.vel = v_add(b.vel, v_mul(accel, (float)dt));
        b.vel = v_limit(b.vel, maxSpeed);

        b.pos = v_add(b.pos, v_mul(b.vel, (float)dt));
        b.pos = wrap_pos(r, b.pos);

        w->boidsNext[i] = b;
    }
}

void world_swap_buffers(World* w) {
    Boid* tmp = w->boids;
    w->boids = w->boidsNext;
    w->boidsNext = tmp;
}
