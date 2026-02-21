#include "update_pthreads.h"

#include <pthread.h>
#include <stdlib.h>

typedef struct WorkerCtx {
    size_t id;
    struct Impl* impl;
} WorkerCtx;

typedef struct Impl {
    size_t threadCount;
    pthread_t* threads;
    WorkerCtx* ctx;

    pthread_mutex_t m;
    pthread_cond_t cvStart;
    pthread_cond_t cvDone;

    pthread_mutex_t bm;
    pthread_cond_t bcv;
    size_t bcount;
    size_t bgen;

    const World* worldRead;
    World* worldWrite;
    double dt;
    Vec2 centroid;
    float detachDist2;
    unsigned char* detached;
    size_t finished;
    bool hasWork;
    bool stop;
} Impl;

static void barrier_wait(Impl* impl) {
    pthread_mutex_lock(&impl->bm);
    if (impl->stop) {
        pthread_mutex_unlock(&impl->bm);
        return;
    }
    size_t gen = impl->bgen;
    impl->bcount++;
    if (impl->bcount == impl->threadCount) {
        impl->bcount = 0;
        impl->bgen++;
        pthread_cond_broadcast(&impl->bcv);
        pthread_mutex_unlock(&impl->bm);
        return;
    }
    while (gen == impl->bgen) {
        pthread_cond_wait(&impl->bcv, &impl->bm);
        if (impl->stop) break;
    }
    pthread_mutex_unlock(&impl->bm);
}

static void* worker_main(void* p) {
    WorkerCtx* ctx = (WorkerCtx*)p;
    Impl* impl = ctx->impl;

    while (true) {
        pthread_mutex_lock(&impl->m);
        while (!impl->hasWork && !impl->stop) {
            pthread_cond_wait(&impl->cvStart, &impl->m);
        }
        if (impl->stop) {
            pthread_mutex_unlock(&impl->m);
            break;
        }

        const World* r = impl->worldRead;
        World* w = impl->worldWrite;
        double dt = impl->dt;
        size_t tc = impl->threadCount;
        size_t id = ctx->id;
        pthread_mutex_unlock(&impl->m);

        size_t begin = (r->boidCount * id) / tc;
        size_t end = (r->boidCount * (id + 1)) / tc;

        world_compute_detached_range(r, impl->centroid, impl->detachDist2, impl->detached, begin, end);
        barrier_wait(impl);
        world_step_range(r, w, begin, end, dt, impl->detached);

        pthread_mutex_lock(&impl->m);
        impl->finished++;
        if (impl->finished == impl->threadCount) {
            impl->hasWork = false;
            pthread_cond_signal(&impl->cvDone);
        }
        pthread_mutex_unlock(&impl->m);
    }

    return NULL;
}

bool update_pthreads_init(UpdatePthreads* u, size_t threadCount) {
    Impl* impl = (Impl*)calloc(1, sizeof(Impl));
    if (!impl) return false;

    impl->threadCount = threadCount;
    impl->threads = (pthread_t*)calloc(threadCount, sizeof(pthread_t));
    impl->ctx = (WorkerCtx*)calloc(threadCount, sizeof(WorkerCtx));
    if (!impl->threads || !impl->ctx) {
        free(impl->threads);
        free(impl->ctx);
        free(impl);
        return false;
    }

    pthread_mutex_init(&impl->m, NULL);
    pthread_cond_init(&impl->cvStart, NULL);
    pthread_cond_init(&impl->cvDone, NULL);

    pthread_mutex_init(&impl->bm, NULL);
    pthread_cond_init(&impl->bcv, NULL);

    for (size_t i = 0; i < threadCount; i++) {
        impl->ctx[i] = (WorkerCtx){.id = i, .impl = impl};
        if (pthread_create(&impl->threads[i], NULL, worker_main, &impl->ctx[i]) != 0) {
            impl->stop = true;
            pthread_cond_broadcast(&impl->cvStart);
            for (size_t j = 0; j < i; j++) pthread_join(impl->threads[j], NULL);
            pthread_mutex_destroy(&impl->m);
            pthread_cond_destroy(&impl->cvStart);
            pthread_cond_destroy(&impl->cvDone);
            free(impl->threads);
            free(impl->ctx);
            free(impl);
            return false;
        }
    }

    u->threadCount = threadCount;
    u->impl = impl;
    return true;
}

void update_pthreads_destroy(UpdatePthreads* u) {
    if (!u || !u->impl) return;
    Impl* impl = (Impl*)u->impl;

    pthread_mutex_lock(&impl->m);
    impl->stop = true;
    pthread_cond_broadcast(&impl->cvStart);
    pthread_mutex_unlock(&impl->m);

    pthread_mutex_lock(&impl->bm);
    pthread_cond_broadcast(&impl->bcv);
    pthread_mutex_unlock(&impl->bm);

    for (size_t i = 0; i < impl->threadCount; i++) {
        pthread_join(impl->threads[i], NULL);
    }

    pthread_mutex_destroy(&impl->m);
    pthread_cond_destroy(&impl->cvStart);
    pthread_cond_destroy(&impl->cvDone);

    pthread_mutex_destroy(&impl->bm);
    pthread_cond_destroy(&impl->bcv);

    free(impl->threads);
    free(impl->ctx);
    free(impl);

    u->impl = NULL;
    u->threadCount = 0;
}

void update_pthreads_step(UpdatePthreads* u, World* w, double dt) {
    Impl* impl = (Impl*)u->impl;

    Vec2 centroid = world_compute_centroid(w);
    float detachDist2 = world_compute_detachDist2(w);

    pthread_mutex_lock(&impl->m);
    impl->worldRead = w;
    impl->worldWrite = w;
    impl->dt = dt;
    impl->centroid = centroid;
    impl->detachDist2 = detachDist2;
    impl->detached = w->detached;
    impl->finished = 0;
    impl->hasWork = true;
    pthread_cond_broadcast(&impl->cvStart);

    while (impl->hasWork) {
        pthread_cond_wait(&impl->cvDone, &impl->m);
    }
    pthread_mutex_unlock(&impl->m);

    world_swap_buffers(w);
}
