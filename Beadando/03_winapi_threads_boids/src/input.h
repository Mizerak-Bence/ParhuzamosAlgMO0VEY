#pragma once

#include <stdbool.h>

typedef struct Input {
    int dummy;
} Input;

typedef struct InputState {
    bool up;
    bool down;
    bool left;
    bool right;
    bool quit;
} InputState;

void input_init(Input* input);
InputState input_poll(Input* input);
