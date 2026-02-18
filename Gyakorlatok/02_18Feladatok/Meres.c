#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void meres(FILE *csv, int n) {
    int *tomb = (int *)malloc(n * sizeof(int));
    if (!tomb) {
        printf("Memoria hiba!\n");
        exit(1);
    }

    clock_t start_gen = clock();
    for (int i = 0; i < n; i++)
        tomb[i] = rand();
    clock_t end_gen = clock();
    double gen_ms = (double)(end_gen - start_gen) / CLOCKS_PER_SEC * 1000.0;

    clock_t start_io = clock();
    clock_t end_io = clock();
    double io_ms = (double)(end_io - start_io) / CLOCKS_PER_SEC * 1000.0;

    fprintf(csv, "%d;%.3f;%.3f\n", n, gen_ms, io_ms);

    free(tomb);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Hasznalat: %s fajlnev max_elemszam\n", argv[0]);
        return 1;
    }

    const char *csv_nev = argv[1];
    int max_n = atoi(argv[2]);

    srand(time(NULL));

    FILE *csv = fopen(csv_nev, "w");
    if (!csv) {
        printf("Nem sikerult letrehozni a fajlt!\n");
        return 1;
    }

    fprintf(csv, "elemszam;generalas_ms;fajl_iras_ms\n");

    for (int n = max_n / 10; n <= max_n; n += max_n / 10)
        meres(csv, n);

    fclose(csv);

    printf("Kesz! Nyisd meg Excelben: %s\n", csv_nev);
    return 0;
}
