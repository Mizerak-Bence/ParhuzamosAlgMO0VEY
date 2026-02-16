#include "input.h"

#include <conio.h>

void input_init(Input* input) {
    (void)input;
}

InputState input_poll(Input* input) {
    (void)input;

    InputState st = {0};

    while (_kbhit()) {
        int c = _getch();
        if (c == 'q' || c == 'Q') st.quit = true;
        if (c == 'w' || c == 'W') st.up = true;
        if (c == 's' || c == 'S') st.down = true;
        if (c == 'a' || c == 'A') st.left = true;
        if (c == 'd' || c == 'D') st.right = true;
    }

    return st;
}
