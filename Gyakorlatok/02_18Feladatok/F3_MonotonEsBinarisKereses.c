#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* A linearis rekurzio a monoton ellenorzesnel nagy n-nel elszallhat (stack overflow).
    Ezert a rekurziv monoton merest csak eddig a hatarig csinaljuk. */
#define REC_LIMIT 20000

static double ms_since(clock_t start, clock_t end) {
    return (double)(end - start) * 1000.0 / (double)CLOCKS_PER_SEC;
}

static double *gen_monotone_double(int n) {
    double *a = (double *)malloc((size_t)n * sizeof(double));
    if (!a) {
        fprintf(stderr, "Nincs eleg memoria\n");
        exit(1);
    }
    double x = 0.0;
    for (int i = 0; i < n; ++i) {
        x += ((double)(rand() % 1000) + 1.0) / 1000.0;
        a[i] = x;
    }
    return a;
}

static int mono_iter(const double *a, int n) {
    for (int i = 1; i < n; ++i) if (!(a[i - 1] < a[i])) return 0;
    return 1;
}

static int mono_rec(const double *a, int i, int n) {
    if (n <= 1) return 1;
    if (i >= n - 1) return 1;
    return (a[i] < a[i + 1]) && mono_rec(a, i + 1, n);
}

static int binsearch_iter(const double *a, int n, double key) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        double v = a[mid];
        if (v == key) return mid;
        if (v < key) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

static int binsearch_rec_impl(const double *a, int lo, int hi, double key) {
    if (lo > hi) return -1;
    int mid = lo + (hi - lo) / 2;
    double v = a[mid];
    if (v == key) return mid;
    if (v < key) return binsearch_rec_impl(a, mid + 1, hi, key);
    return binsearch_rec_impl(a, lo, mid - 1, key);
}

static int binsearch_rec(const double *a, int n, double key) {
    return binsearch_rec_impl(a, 0, n - 1, key);
}

static void usage(const char *exe) {
    printf("Hasznalat:\n");
    printf("  %s scale <csv_kimenet> <max_n> <probak_szama_nenkent>\n", exe);
    printf("  %s dist  <csv_kimenet> <n> <probak_szama>\n", exe);
    printf("\n");
    printf("scale CSV oszlopok: n;mono_iter_ms;mono_rec_ms;bs_iter_avg_ms;bs_rec_avg_ms;bs_iter_max_ms;bs_rec_max_ms\n");
    printf("dist  CSV oszlopok: trial;present;iter_ms;rec_ms;iter_found;rec_found\n");
    printf("Megjegyzes: a rekurziv monoton ellenorzes merese csak n=%d-ig megy (utana -1).\n", REC_LIMIT);
}

static void mode_dist(const char *csv_out, int n, int trials) {
    FILE *csv = fopen(csv_out, "w");
    if (!csv) {
        fprintf(stderr, "Nem tudom megnyitni: '%s'\n", csv_out);
        exit(1);
    }
    fprintf(csv, "trial;present;iter_ms;rec_ms;iter_found;rec_found\n");

    double *a = gen_monotone_double(n);

    for (int t = 0; t < trials; ++t) {
        int present = (rand() & 1);
        double key = present ? a[rand() % n] : (a[rand() % n] + 0.12345);

        clock_t t0 = clock();
        int it = binsearch_iter(a, n, key);
        clock_t t1 = clock();
        double it_ms = ms_since(t0, t1);

        t0 = clock();
        int rc = binsearch_rec(a, n, key);
        t1 = clock();
        double rc_ms = ms_since(t0, t1);

        fprintf(csv, "%d;%d;%.6f;%.6f;%d;%d\n", t, present, it_ms, rc_ms, (it >= 0), (rc >= 0));
    }

    free(a);
    fclose(csv);
    printf("Kesz: %s\n", csv_out);
}

static void mode_scale(const char *csv_out, int max_n, int trials_per_n) {
    FILE *csv = fopen(csv_out, "w");
    if (!csv) {
        fprintf(stderr, "Nem tudom megnyitni: '%s'\n", csv_out);
        exit(1);
    }
    fprintf(csv, "n;mono_iter_ms;mono_rec_ms;bs_iter_avg_ms;bs_rec_avg_ms;bs_iter_max_ms;bs_rec_max_ms\n");

    for (int step = 1; step <= 10; ++step) {
        int n = (max_n * step) / 10;
        if (n < 2) n = 2;

        double *a = gen_monotone_double(n);

        clock_t t0 = clock();
        (void)mono_iter(a, n);
        clock_t t1 = clock();
        double mono_it = ms_since(t0, t1);

        double mono_rc = -1.0;
        if (n <= REC_LIMIT) {
            t0 = clock();
            (void)mono_rec(a, 0, n);
            t1 = clock();
            mono_rc = ms_since(t0, t1);
        }

        double it_sum = 0.0, rc_sum = 0.0;
        double it_max = 0.0, rc_max = 0.0;

        for (int t = 0; t < trials_per_n; ++t) {
            int present = (rand() & 1);
            double key = present ? a[rand() % n] : (a[rand() % n] + 0.12345);

            t0 = clock();
            (void)binsearch_iter(a, n, key);
            t1 = clock();
            double it_ms = ms_since(t0, t1);

            t0 = clock();
            (void)binsearch_rec(a, n, key);
            t1 = clock();
            double rc_ms = ms_since(t0, t1);

            it_sum += it_ms;
            rc_sum += rc_ms;
            if (it_ms > it_max) it_max = it_ms;
            if (rc_ms > rc_max) rc_max = rc_ms;
        }

        fprintf(csv, "%d;%.6f;%.6f;%.6f;%.6f;%.6f;%.6f\n",
                n,
                mono_it,
                mono_rc,
                it_sum / (double)trials_per_n,
                rc_sum / (double)trials_per_n,
                it_max,
                rc_max);

        free(a);
    }

    fclose(csv);
    printf("Kesz: %s\n", csv_out);
}

int main(int argc, char **argv) {
    srand((unsigned)time(NULL));

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "dist") == 0) {
        if (argc != 5) {
            usage(argv[0]);
            return 1;
        }
        int n = atoi(argv[3]);
        int trials = atoi(argv[4]);
        if (n <= 1 || trials <= 0) {
            fprintf(stderr, "Hibas n vagy probak_szama\n");
            return 1;
        }
        mode_dist(argv[2], n, trials);
        return 0;
    }

    if (strcmp(argv[1], "scale") == 0) {
        if (argc != 5) {
            usage(argv[0]);
            return 1;
        }
        int max_n = atoi(argv[3]);
        int trials = atoi(argv[4]);
        if (max_n <= 1 || trials <= 0) {
            fprintf(stderr, "Hibas max_n vagy probak_szama\n");
            return 1;
        }
        mode_scale(argv[2], max_n, trials);
        return 0;
    }

    usage(argv[0]);
    return 1;
}
