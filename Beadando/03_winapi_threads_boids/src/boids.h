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
    Player player;
} World;

bool world_init(World* world, int width, int height, size_t boidCount);
void world_destroy(World* world);

struct InputState;
void world_apply_player_input(World* world, const struct InputState* input, double dt);

void world_step_range(const World* worldRead, World* worldWrite, size_t begin, size_t end, double dt);
