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

static Vec2 clamp_pos(const World* w, Vec2 p) {
    if (p.x < 0) p.x = 0;
    if (p.y < 0) p.y = 0;
    if (p.x > (float)(w->width - 1)) p.x = (float)(w->width - 1);
    if (p.y > (float)(w->height - 1)) p.y = (float)(w->height - 1);
    return p;
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
    w->boidCount = boidCount;

    w->boids = (Boid*)calloc(boidCount, sizeof(Boid));
    w->boidsNext = (Boid*)calloc(boidCount, sizeof(Boid));
    if (!w->boids || !w->boidsNext) {
        free(w->boids);
        free(w->boidsNext);
        return false;
    }

    const int groupCount = (boidCount >= 300) ? 6 : 5;
    Vec2 centers[8];
    Vec2 dirs[8];
    for (int g = 0; g < groupCount; g++) {
        centers[g] = (Vec2){
            frand_range(0.15f * (float)(width - 1), 0.85f * (float)(width - 1)),
            frand_range(0.15f * (float)(height - 1), 0.85f * (float)(height - 1)),
        };
        float a = frand_range(0.0f, 6.2831853f);
        dirs[g] = (Vec2){cosf(a), sinf(a)};
    }

    const float spreadX = (float)width * 0.08f;
    const float spreadY = (float)height * 0.12f;
    const float baseSpeed = 14.0f;

    for (size_t i = 0; i < boidCount; i++) {
        int g = (int)(i % (size_t)groupCount);
        Vec2 p = v_add(centers[g], (Vec2){frand_signed() * spreadX, frand_signed() * spreadY});
        p = clamp_pos(w, p);
        w->boids[i].pos = p;

        float sp = baseSpeed + frand_signed() * 4.0f;
        Vec2 jitterDir = v_norm((Vec2){dirs[g].x + frand_signed() * 0.25f, dirs[g].y + frand_signed() * 0.25f});
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
        w->player.pos = clamp_pos(w, w->player.pos);
    }
}

void world_step_range(const World* r, World* w, size_t begin, size_t end, double dt) {
    const float neighborRadius = 5.8f;
    const float neighborRadius2 = neighborRadius * neighborRadius;
    const float desiredSeparation = 2.2f;
    const float desiredSeparation2 = desiredSeparation * desiredSeparation;

    const float maxSpeed = 30.0f;
    const float maxForce = 25.0f;

    const float wCohesion = 0.45f;
    const float wAlignment = 0.85f;
    const float wSeparation = 1.45f;
    const float wAvoidPlayer = 1.50f;
    const float wWalls = 1.20f;

    const float wallMargin = 4.0f;

    for (size_t i = begin; i < end; i++) {
        Boid b = r->boids[i];

        Vec2 sumPos = {0, 0};
        Vec2 sumVel = {0, 0};
        Vec2 sumSep = {0, 0};
        int neighbors = 0;

        for (size_t j = 0; j < r->boidCount; j++) {
            if (i == j) continue;
            Vec2 diff = v_sub(r->boids[j].pos, b.pos);
            float d2 = v_len2(diff);
            if (d2 < neighborRadius2) {
                neighbors++;
                sumPos = v_add(sumPos, r->boids[j].pos);
                sumVel = v_add(sumVel, r->boids[j].vel);

                if (d2 < desiredSeparation2 && d2 > 1e-6f) {
                    Vec2 away = v_mul(v_norm(diff), -1.0f);
                    sumSep = v_add(sumSep, v_div(away, sqrtf(d2)));
                }
            }
        }

        Vec2 accel = {0, 0};

        if (neighbors > 0) {
            Vec2 center = v_mul(sumPos, 1.0f / (float)neighbors);
            Vec2 avgVel = v_mul(sumVel, 1.0f / (float)neighbors);

            Vec2 desiredC = v_mul(v_norm(v_sub(center, b.pos)), maxSpeed);
            Vec2 desiredA = v_mul(v_norm(avgVel), maxSpeed);
            Vec2 desiredS = v_mul(v_norm(sumSep), maxSpeed);

            Vec2 coh = steer_towards(desiredC, b.vel, maxForce);
            Vec2 ali = steer_towards(desiredA, b.vel, maxForce);
            Vec2 sep = steer_towards(desiredS, b.vel, maxForce);

            accel = v_add(accel, v_mul(coh, wCohesion));
            accel = v_add(accel, v_mul(ali, wAlignment));
            accel = v_add(accel, v_mul(sep, wSeparation));
        }

        Vec2 toPlayer = v_sub(r->player.pos, b.pos);
        float dp2 = v_len2(toPlayer);
        const float playerAvoidRadius = 10.0f;
        if (dp2 < playerAvoidRadius * playerAvoidRadius && dp2 > 1e-6f) {
            Vec2 desired = v_mul(v_norm(v_mul(toPlayer, -1.0f)), maxSpeed);
            Vec2 avoid = steer_towards(desired, b.vel, maxForce);
            accel = v_add(accel, v_mul(avoid, wAvoidPlayer));
        }

        Vec2 wallDesired = {0, 0};
        if (b.pos.x < wallMargin) wallDesired.x = maxSpeed;
        else if (b.pos.x > (float)(r->width - 1) - wallMargin) wallDesired.x = -maxSpeed;
        if (b.pos.y < wallMargin) wallDesired.y = maxSpeed;
        else if (b.pos.y > (float)(r->height - 1) - wallMargin) wallDesired.y = -maxSpeed;
        if (wallDesired.x != 0 || wallDesired.y != 0) {
            Vec2 wallSteer = steer_towards(v_mul(v_norm(wallDesired), maxSpeed), b.vel, maxForce);
            accel = v_add(accel, v_mul(wallSteer, wWalls));
        }

        b.vel = v_add(b.vel, v_mul(accel, (float)dt));
        b.vel = v_limit(b.vel, maxSpeed);

        b.pos = v_add(b.pos, v_mul(b.vel, (float)dt));

        if (b.pos.x < 0) { b.pos.x = 0; b.vel.x *= -0.6f; }
        if (b.pos.y < 0) { b.pos.y = 0; b.vel.y *= -0.6f; }
        if (b.pos.x > (float)(r->width - 1)) { b.pos.x = (float)(r->width - 1); b.vel.x *= -0.6f; }
        if (b.pos.y > (float)(r->height - 1)) { b.pos.y = (float)(r->height - 1); b.vel.y *= -0.6f; }

        w->boidsNext[i] = b;
    }
}

void world_swap_buffers(World* w) {
    Boid* tmp = w->boids;
    w->boids = w->boidsNext;
    w->boidsNext = tmp;
}
