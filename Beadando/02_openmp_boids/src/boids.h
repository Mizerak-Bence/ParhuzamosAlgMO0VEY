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
    unsigned char group;
    unsigned char alive;
    unsigned char predator;
} Boid;

typedef struct Player {
    Vec2 pos;
    float speed;
} Player;

typedef struct InputState {
    bool up;
    bool down;
    bool left;
    bool right;
    bool quit;
} InputState;

typedef struct World {
    int width;
    int height;
    int groupCount;
    size_t boidCount;
    Boid* boids;
    Boid* boidsNext;
    Player player;
} World;

bool world_init(World* world, int width, int height, size_t boidCount);
void world_destroy(World* world);

void world_apply_player_input(World* world, const InputState* input, double dt);

double world_step_seq(World* world, double dt);
double world_step_openmp(World* world, int threads, double dt);