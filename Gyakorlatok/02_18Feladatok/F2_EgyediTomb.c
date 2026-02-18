#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double ms_since(clock_t start, clock_t end) {
    return (double)(end - start) * 1000.0 / (double)CLOCKS_PER_SEC;
}

static void shuffle_int(int *a, int n) {
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int t = a[i];
        a[i] = a[j];
        a[j] = t;
    }
}

static int all_unique_n2(const int *a, int n) {
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (a[i] == a[j]) return 0;
        }
    }
    return 1;
}

static void usage(const char *exe) {
    printf("Hasznalat: %s <csv_kimenet> <max_n>\n", exe);
    printf("CSV oszlopok: n;gen_unique_ms;dup_ms;unique_check_ms;all_unique\n");
    printf("Megjegyzes: az egyediseg-ellenorzes O(n^2), szoval ne legyen tul nagy a max_n.\n");
}

int main(int argc, char **argv) {
    if (argc != 3) {
        usage(argv[0]);
        return 1;
    }

    int max_n = atoi(argv[2]);
    if (max_n <= 0) {
        fprintf(stderr, "Hibas max_n\n");
        return 1;
    }

    srand((unsigned)time(NULL));

    FILE *csv = fopen(argv[1], "w");
    if (!csv) {
        fprintf(stderr, "Nem tudom megnyitni: '%s'\n", argv[1]);
        return 1;
    }

    fprintf(csv, "n;gen_unique_ms;dup_ms;unique_check_ms;all_unique\n");

    for (int step = 1; step <= 10; ++step) {
        int n = (max_n * step) / 10;
        if (n < 1) n = 1;

        int *a = (int *)malloc((size_t)n * sizeof(int));
        if (!a) {
            fprintf(stderr, "Nincs eleg memoria\n");
            fclose(csv);
            return 1;
        }

        clock_t t0 = clock();
        for (int i = 0; i < n; ++i) a[i] = i;
        shuffle_int(a, n);
        clock_t t1 = clock();
        double gen_ms = ms_since(t0, t1);

        t0 = clock();
        for (int i = 0; i < n; ++i) {
            if ((rand() & 1) == 0) {
                a[i] = a[rand() % n];
            }
        }
        t1 = clock();
        double dup_ms = ms_since(t0, t1);

        t0 = clock();
        int ok = all_unique_n2(a, n);
        t1 = clock();
        double chk_ms = ms_since(t0, t1);

        fprintf(csv, "%d;%.6f;%.6f;%.6f;%d\n", n, gen_ms, dup_ms, chk_ms, ok);
        free(a);
    }

    fclose(csv);
    printf("Kesz: %s\n", argv[1]);
    return 0;
}
