#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

/* ================= CREW ================= */

void crew_prefix(int* a, int n)
{
    int* temp = malloc(n * sizeof(int));

    for (int d = 1; d < n; d <<= 1) {
        #pragma omp parallel for
        for (int i = 0; i < n; i++) {
            if (i >= d)
                temp[i] = a[i] + a[i - d];
            else
                temp[i] = a[i];
        }
        #pragma omp parallel for
        for (int i = 0; i < n; i++)
            a[i] = temp[i];
    }
    free(temp);
}

/* ================= EREW ================= */

void erew_prefix(int* a, int n)
{
    int* temp = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) temp[i] = a[i];

    for (int d = 1; d < n; d <<= 1) {
        #pragma omp parallel for
        for (int i = 0; i < n; i += 2 * d)
            if (i + 2 * d - 1 < n)
                temp[i + 2 * d - 1] += temp[i + d - 1];
    }

    temp[n - 1] = 0;

    for (int d = n >> 1; d >= 1; d >>= 1) {
        #pragma omp parallel for
        for (int i = 0; i < n; i += 2 * d)
            if (i + 2 * d - 1 < n) {
                int t = temp[i + d - 1];
                temp[i + d - 1] = temp[i + 2 * d - 1];
                temp[i + 2 * d - 1] += t;
            }
    }

    #pragma omp parallel for
    for (int i = 0; i < n; i++)
        a[i] = temp[i] + a[i];

    free(temp);
}

/* ================= OPTIMAL ================= */

void optimal_prefix(int* a, int n)
{
    int p = omp_get_max_threads();
    if (p > n) p = n;

    int block = (n + p - 1) / p;
    int* sums = malloc(p * sizeof(int));

    /* blokkon belüli prefix */
    #pragma omp parallel
    {
        int id = omp_get_thread_num();

        if (id < p) {
            int start = id * block;
            int end = start + block;
            if (end > n) end = n;

            for (int i = start + 1; i < end; i++)
                a[i] += a[i - 1];

            sums[id] = (start < n) ? a[end - 1] : 0;
        }
    }

    /* blokkok prefixe – szekvenciális */
    for (int i = 1; i < p; i++)
        sums[i] += sums[i - 1];

    /* korrekció */
    #pragma omp parallel
    {
        int id = omp_get_thread_num();

        if (id > 0 && id < p) {
            int start = id * block;
            int end = start + block;
            if (end > n) end = n;

            int add = sums[id - 1];
            for (int i = start; i < end; i++)
                a[i] += add;
        }
    }

    free(sums);
}

/* ================= MERES ================= */

double measure(void (*prefix)(int*, int), int n)
{
    int* a = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) a[i] = 1;

    double t0 = omp_get_wtime();
    prefix(a, n);
    double t1 = omp_get_wtime();

    free(a);
    return t1 - t0;
}

/* ================= MAIN ================= */

int main(void)
{
    int sizes[] = {100000, 500000, 1000000};
    int count = sizeof(sizes) / sizeof(sizes[0]);

    printf("n,CREW,EREW,OPTIMAL\n");

    for (int i = 0; i < count; i++) {
        int n = sizes[i];

        double t_crew    = measure(crew_prefix, n);
        double t_erew    = measure(erew_prefix, n);
        double t_optimal = measure(optimal_prefix, n);

        printf("%d,%.6f,%.6f,%.6f\n",
               n, t_crew, t_erew, t_optimal);
    }

    return 0;
}