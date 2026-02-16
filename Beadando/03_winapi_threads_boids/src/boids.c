#include "boids.h"

#include "input.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static float frand01(void) { return (float)rand() / (float)RAND_MAX; }

static Vec2 v_add(Vec2 a, Vec2 b) { return (Vec2){a.x + b.x, a.y + b.y}; }
static Vec2 v_sub(Vec2 a, Vec2 b) { return (Vec2){a.x - b.x, a.y - b.y}; }
static Vec2 v_mul(Vec2 a, float k) { return (Vec2){a.x * k, a.y * k}; }
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

    for (size_t i = 0; i < boidCount; i++) {
        w->boids[i].pos.x = frand01() * (float)(width - 1);
        w->boids[i].pos.y = frand01() * (float)(height - 1);
        w->boids[i].vel.x = (frand01() - 0.5f) * 10.0f;
        w->boids[i].vel.y = (frand01() - 0.5f) * 10.0f;
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
        w->player.pos = clamp_pos(w, w->player.pos);
    }
}

void world_step_range(const World* r, World* w, size_t begin, size_t end, double dt) {
    const float neighborRadius = 6.0f;
    const float neighborRadius2 = neighborRadius * neighborRadius;
    const float sepRadius = 2.0f;
    const float sepRadius2 = sepRadius * sepRadius;

    const float cohesionWeight = 0.05f;
    const float alignWeight = 0.10f;
    const float sepWeight = 0.20f;
    const float avoidPlayerWeight = 0.50f;

    const float maxSpeed = 35.0f;

    for (size_t i = begin; i < end; i++) {
        Boid b = r->boids[i];

        Vec2 center = {0, 0};
        Vec2 avgVel = {0, 0};
        Vec2 sep = {0, 0};
        int neighbors = 0;

        for (size_t j = 0; j < r->boidCount; j++) {
            if (i == j) continue;
            Vec2 diff = v_sub(r->boids[j].pos, b.pos);
            float d2 = v_len2(diff);
            if (d2 < neighborRadius2) {
                center = v_add(center, r->boids[j].pos);
                avgVel = v_add(avgVel, r->boids[j].vel);
                neighbors++;
                if (d2 < sepRadius2 && d2 > 1e-6f) {
                    sep = v_sub(sep, v_mul(diff, 1.0f / (d2)));
                }
            }
        }

        Vec2 accel = {0, 0};
        if (neighbors > 0) {
            center = v_mul(center, 1.0f / (float)neighbors);
            avgVel = v_mul(avgVel, 1.0f / (float)neighbors);

            Vec2 cohesion = v_sub(center, b.pos);
            Vec2 align = v_sub(avgVel, b.vel);

            accel = v_add(accel, v_mul(cohesion, cohesionWeight));
            accel = v_add(accel, v_mul(align, alignWeight));
            accel = v_add(accel, v_mul(sep, sepWeight));
        }

        Vec2 toPlayer = v_sub(r->player.pos, b.pos);
        float dp2 = v_len2(toPlayer);
        if (dp2 < (neighborRadius2 * 4.0f) && dp2 > 1e-6f) {
            accel = v_sub(accel, v_mul(v_norm(toPlayer), avoidPlayerWeight));
        }

        b.vel = v_add(b.vel, v_mul(accel, (float)dt * 60.0f));
        float sp = v_len(b.vel);
        if (sp > maxSpeed) {
            b.vel = v_mul(b.vel, maxSpeed / sp);
        }

        b.pos = v_add(b.pos, v_mul(b.vel, (float)dt));

        if (b.pos.x < 0) { b.pos.x = 0; b.vel.x *= -0.6f; }
        if (b.pos.y < 0) { b.pos.y = 0; b.vel.y *= -0.6f; }
        if (b.pos.x > (float)(r->width - 1)) { b.pos.x = (float)(r->width - 1); b.vel.x *= -0.6f; }
        if (b.pos.y > (float)(r->height - 1)) { b.pos.y = (float)(r->height - 1); b.vel.y *= -0.6f; }

        w->boidsNext[i] = b;
    }
}
