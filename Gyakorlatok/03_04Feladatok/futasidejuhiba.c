#include <stdio.h>
#include <pthread.h>

void* crash_thread(void* arg)
{
    printf("Szál: hibas memoriahozzaferes...\n");
    int* p = NULL;
    *p = 42;   // SZEGMENTACIOS HIBA
    return NULL;
}

int main(void)
{
    pthread_t tid;

    pthread_create(&tid, NULL, crash_thread, NULL);

    pthread_join(tid, NULL);

    printf("Ez a sor mar nem fut le\n");
    return 0;
}