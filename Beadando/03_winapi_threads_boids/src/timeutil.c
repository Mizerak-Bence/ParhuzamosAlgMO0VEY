#include "timeutil.h"

#include <windows.h>

uint64_t time_now_us(void) {
    static LARGE_INTEGER freq;
    static int init = 0;
    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = 1;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart * 1000000ULL) / (uint64_t)freq.QuadPart);
}

void time_sleep_us(uint64_t us) {
    if (us == 0) return;
    DWORD ms = (DWORD)(us / 1000ULL);
    if (ms == 0) ms = 1;
    Sleep(ms);
}
