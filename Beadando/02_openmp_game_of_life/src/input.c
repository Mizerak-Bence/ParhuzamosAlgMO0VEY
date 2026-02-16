#include "input.h"

#ifdef _WIN32
#include <conio.h>
#endif

void input_init(Input* input) {
    (void)input;
}

InputState input_poll(Input* input) {
    (void)input;

    InputState st = {0};
#ifdef _WIN32
    while (_kbhit()) {
        int c = _getch();
        if (c == 'q' || c == 'Q') st.quit = true;
        if (c == 'w' || c == 'W') st.up = true;
        if (c == 's' || c == 'S') st.down = true;
        if (c == 'a' || c == 'A') st.left = true;
        if (c == 'd' || c == 'D') st.right = true;
        if (c == ' ') st.toggle = true;
        if (c == 'p' || c == 'P') st.pauseToggle = true;
    }
#endif
    return st;
}
