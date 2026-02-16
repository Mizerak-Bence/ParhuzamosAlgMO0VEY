#include "update_winapi.h"

#include <windows.h>

#include <stdlib.h>

typedef struct Impl Impl;

typedef struct Worker {
    size_t id;
    Impl* impl;
    HANDLE thread;
    HANDLE evStart;
} Worker;

struct Impl {
    size_t threadCount;
    Worker* workers;
    HANDLE evDone;

    volatile LONG finished;
    volatile LONG stop;

    World* world;
    double dt;
};

static DWORD WINAPI worker_main(LPVOID p) {
    Worker* w = (Worker*)p;
    Impl* impl = w->impl;

    for (;;) {
        WaitForSingleObject(w->evStart, INFINITE);
        if (impl->stop) break;

        World* world = impl->world;
        double dt = impl->dt;

        size_t tc = impl->threadCount;
        size_t id = w->id;
        size_t begin = (world->boidCount * id) / tc;
        size_t end = (world->boidCount * (id + 1)) / tc;

        world_step_range(world, world, begin, end, dt);

        LONG done = InterlockedIncrement(&impl->finished);
        if ((size_t)done == impl->threadCount) {
            SetEvent(impl->evDone);
        }
    }

    return 0;
}

bool update_winapi_init(UpdateWinApi* u, size_t threadCount) {
    Impl* impl = (Impl*)calloc(1, sizeof(Impl));
    if (!impl) return false;

    impl->threadCount = threadCount;
    impl->workers = (Worker*)calloc(threadCount, sizeof(Worker));
    if (!impl->workers) {
        free(impl);
        return false;
    }

    impl->evDone = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!impl->evDone) {
        if (impl->evDone) CloseHandle(impl->evDone);
        free(impl->workers);
        free(impl);
        return false;
    }

    for (size_t i = 0; i < threadCount; i++) {
        impl->workers[i].id = i;
        impl->workers[i].impl = impl;

        impl->workers[i].evStart = CreateEventA(NULL, FALSE, FALSE, NULL); // auto-reset
        if (!impl->workers[i].evStart) {
            impl->stop = 1;
            for (size_t j = 0; j < i; j++) SetEvent(impl->workers[j].evStart);
            for (size_t j = 0; j < i; j++) WaitForSingleObject(impl->workers[j].thread, INFINITE);
            for (size_t j = 0; j < i; j++) CloseHandle(impl->workers[j].thread);
            for (size_t j = 0; j < i; j++) CloseHandle(impl->workers[j].evStart);
            CloseHandle(impl->evDone);
            free(impl->workers);
            free(impl);
            return false;
        }

        impl->workers[i].thread = CreateThread(NULL, 0, worker_main, &impl->workers[i], 0, NULL);
        if (!impl->workers[i].thread) {
            impl->stop = 1;
            for (size_t j = 0; j <= i; j++) SetEvent(impl->workers[j].evStart);
            for (size_t j = 0; j < i; j++) WaitForSingleObject(impl->workers[j].thread, INFINITE);
            for (size_t j = 0; j < i; j++) CloseHandle(impl->workers[j].thread);
            for (size_t j = 0; j <= i; j++) CloseHandle(impl->workers[j].evStart);
            CloseHandle(impl->evDone);
            free(impl->workers);
            free(impl);
            return false;
        }
    }

    u->threadCount = threadCount;
    u->impl = impl;
    return true;
}

void update_winapi_destroy(UpdateWinApi* u) {
    if (!u || !u->impl) return;
    Impl* impl = (Impl*)u->impl;

    impl->stop = 1;
    for (size_t i = 0; i < impl->threadCount; i++) {
        SetEvent(impl->workers[i].evStart);
    }

    for (size_t i = 0; i < impl->threadCount; i++) {
        WaitForSingleObject(impl->workers[i].thread, INFINITE);
        CloseHandle(impl->workers[i].thread);
        CloseHandle(impl->workers[i].evStart);
    }

    CloseHandle(impl->evDone);
    free(impl->workers);
    free(impl);

    u->impl = NULL;
    u->threadCount = 0;
}

void update_winapi_step(UpdateWinApi* u, World* w, double dt) {
    Impl* impl = (Impl*)u->impl;
    impl->world = w;
    impl->dt = dt;
    impl->finished = 0;

    ResetEvent(impl->evDone);
    for (size_t i = 0; i < impl->threadCount; i++) {
        SetEvent(impl->workers[i].evStart);
    }

    WaitForSingleObject(impl->evDone, INFINITE);

    Boid* tmp = w->boids;
    w->boids = w->boidsNext;
    w->boidsNext = tmp;
}
