#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define THREADS 60

void* work(void* arg)
{
    sleep(1);   // 1 mp számítás
    return NULL;
}

int main(void)
{
    pthread_t tid[THREADS];
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < THREADS; i++)
        pthread_create(&tid[i], NULL, work, NULL);

    for (int i = 0; i < THREADS; i++)
        pthread_join(tid[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed =
        (end.tv_sec - start.tv_sec) +
        (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Teljes futasi ido: %.2f masodperc\n", elapsed);

    return 0;
}