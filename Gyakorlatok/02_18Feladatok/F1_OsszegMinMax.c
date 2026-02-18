#include <stdio.h>
#include <stdlib.h>
#include <time.h>


#define REC_LIMIT 20000

static double ms_since(clock_t start, clock_t end) {
    return (double)(end - start) * 1000.0 / (double)CLOCKS_PER_SEC;
}

static long long sum_iter(const int *a, int n) {
    long long s = 0;
    for (int i = 0; i < n; ++i) s += a[i];
    return s;
}

static int min_iter(const int *a, int n) {
    int m = a[0];
    for (int i = 1; i < n; ++i) if (a[i] < m) m = a[i];
    return m;
}

static int max_iter(const int *a, int n) {
    int m = a[0];
    for (int i = 1; i < n; ++i) if (a[i] > m) m = a[i];
    return m;
}

static long long sum_rec(const int *a, int n) {
    if (n <= 0) return 0;
    return (long long)a[0] + sum_rec(a + 1, n - 1);
}

static int min_rec(const int *a, int n) {
    if (n == 1) return a[0];
    int m = min_rec(a + 1, n - 1);
    return (a[0] < m) ? a[0] : m;
}

static int max_rec(const int *a, int n) {
    if (n == 1) return a[0];
    int m = max_rec(a + 1, n - 1);
    return (a[0] > m) ? a[0] : m;
}

static void usage(const char *exe) {
    printf("Hasznalat: %s <csv_kimenet> <max_n>\n", exe);
    printf("CSV oszlopok: n;sum_iter_ms;sum_rec_ms;min_iter_ms;min_rec_ms;max_iter_ms;max_rec_ms\n");
    printf("Megjegyzes: a rekurziv meres csak n=%d-ig megy (utana -1 lesz beirva).\n", REC_LIMIT);
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

    fprintf(csv, "n;sum_iter_ms;sum_rec_ms;min_iter_ms;min_rec_ms;max_iter_ms;max_rec_ms\n");

    for (int step = 1; step <= 10; ++step) {
        int n = (max_n * step) / 10;
        if (n < 1) n = 1;

        int *a = (int *)malloc((size_t)n * sizeof(int));
        if (!a) {
            fprintf(stderr, "Nincs eleg memoria\n");
            fclose(csv);
            return 1;
        }
        for (int i = 0; i < n; ++i) a[i] = rand();

        clock_t t0, t1;

        t0 = clock();
        (void)sum_iter(a, n);
        t1 = clock();
        double sum_it = ms_since(t0, t1);

        t0 = clock();
        (void)min_iter(a, n);
        t1 = clock();
        double min_it = ms_since(t0, t1);

        t0 = clock();
        (void)max_iter(a, n);
        t1 = clock();
        double max_it = ms_since(t0, t1);

        double sum_rc = -1.0, min_rc = -1.0, max_rc = -1.0;
        if (n <= REC_LIMIT) {
            t0 = clock();
            (void)sum_rec(a, n);
            t1 = clock();
            sum_rc = ms_since(t0, t1);

            t0 = clock();
            (void)min_rec(a, n);
            t1 = clock();
            min_rc = ms_since(t0, t1);

            t0 = clock();
            (void)max_rec(a, n);
            t1 = clock();
            max_rc = ms_since(t0, t1);
        }

        fprintf(csv, "%d;%.6f;%.6f;%.6f;%.6f;%.6f;%.6f\n",
                n, sum_it, sum_rc, min_it, min_rc, max_it, max_rc);

        free(a);
    }

    fclose(csv);
    printf("Kesz: %s\n", argv[1]);
    return 0;
}
