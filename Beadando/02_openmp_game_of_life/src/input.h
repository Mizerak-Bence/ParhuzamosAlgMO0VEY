#pragma once

#include <stdbool.h>

typedef struct Input {
    int dummy;
} Input;

typedef struct InputState {
    bool up, down, left, right;
    bool toggle;
    bool pauseToggle;
    bool quit;
} InputState;

void input_init(Input* input);
InputState input_poll(Input* input);
