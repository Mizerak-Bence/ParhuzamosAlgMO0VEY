#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct Vec2 {
    float x;
    float y;
} Vec2;

typedef struct Boid {
    Vec2 pos;
    Vec2 vel;
} Boid;

typedef struct Player {
    Vec2 pos;
    float speed;
} Player;

typedef struct World {
    int width;
    int height;
    size_t boidCount;
    Boid* boids;
    Boid* boidsNext;
    unsigned char* detached; /* 0/1 flags, recalculated each tick */
    Player player;
} World;

typedef struct InputState {
    bool up;
    bool down;
    bool left;
    bool right;
} InputState;

bool world_init(World* world, int width, int height, size_t boidCount);
void world_destroy(World* world);

void world_apply_player_input(World* world, const InputState* input, double dt);

Vec2 world_compute_centroid(const World* world);
float world_compute_detachDist2(const World* world);
void world_compute_detached_range(const World* world, Vec2 centroid, float detachDist2, unsigned char* detached, size_t begin, size_t end);

void world_step_range(const World* worldRead, World* worldWrite, size_t begin, size_t end, double dt, const unsigned char* detached);

void world_swap_buffers(World* world);
