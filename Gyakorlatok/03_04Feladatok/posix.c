#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

/* Gyerek szal */
void* worker(void* arg)
{
    int seconds = *(int*)arg;

    printf("  [Gyerek szal] Szamitas kezdete (%d mp)\n", seconds);
    sleep(seconds);
    printf("  [Gyerek szal] Szamitas vege\n");

    return NULL;
}

int main(void)
{
    pthread_t tid;
    int child_time;
    
    /* -------- 1. ESET -------- */
    printf("\n=== 1. ESET: fo szal 8 mp, gyerek szal 4 mp ===\n");

    child_time = 4;
    pthread_create(&tid, NULL, worker, &child_time);

    printf("[Fo szal] Szamitas kezdete (8 mp)\n");
    sleep(8);
    printf("[Fo szal] Szamitas vege\n");

    pthread_join(tid, NULL);
    printf("[Fo szal] Gyerek szal befejezodott\n");

    /* -------- 2. ESET -------- */
    printf("\n=== 2. ESET: fo szal 4 mp, gyerek szal 8 mp ===\n");

    child_time = 8;
    pthread_create(&tid, NULL, worker, &child_time);

    printf("[Fo szal] Szamitas kezdete (4 mp)\n");
    sleep(4);
    printf("[Fo szal] Szamitas vege\n");

    pthread_join(tid, NULL);
    printf("[Fo szal] Gyerek szal befejezodott\n");

    return 0;
}