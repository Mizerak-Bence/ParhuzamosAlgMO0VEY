#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <omp.h>
#include <time.h>

#define N 1000000
#define MAX_DEPTH 3
#define NUM_THREADS 8

double *a;

/* ---------------- 1. SZEKVENCIÁLIS ---------------- */
double product_seq(int start, int end)
{
    double prod = 1.0;
    for (int i = start; i < end; i++)
        prod *= a[i];
    return prod;
}

/* ---------------- 2. REKURZÍV ---------------- */
double product_recursive(int start, int end, int depth)
{
    if (depth == 0 || end - start <= 1)
        return product_seq(start, end);

    int mid = (start + end) / 2;
    return product_recursive(start, mid, depth - 1) *
           product_recursive(mid, end, depth - 1);
}

/* ---------------- 3. POSIX SZÁLAK ---------------- */
typedef struct {
    int start, end, depth;
    double result;
} thread_data;

void* thread_func(void* arg)
{
    thread_data* d = (thread_data*)arg;

    if (d->depth == 0 || d->end - d->start <= 1) {
        d->result = product_seq(d->start, d->end);
        return NULL;
    }

    int mid = (d->start + d->end) / 2;

    thread_data left = {d->start, mid, d->depth - 1, 1.0};
    thread_data right = {mid, d->end, d->depth - 1, 1.0};

    pthread_t t;
    pthread_create(&t, NULL, thread_func, &left);
    thread_func(&right);
    pthread_join(t, NULL);

    d->result = left.result * right.result;
    return NULL;
}

/* ---------------- 4. OPENMP PARALLEL FOR ---------------- */
double product_omp_for()
{
    double prod = 1.0;

    #pragma omp parallel
    {
        double local = 1.0;

        #pragma omp for
        for (int i = 0; i < N; i++)
            local *= a[i];

        #pragma omp critical
        prod *= local;
    }
    return prod;
}

/* ---------------- 5. OPENMP REDUCTION ---------------- */
double product_omp_reduction()
{
    double prod = 1.0;

    #pragma omp parallel for reduction(*:prod)
    for (int i = 0; i < N; i++)
        prod *= a[i];

    return prod;
}

/* ---------------- MAIN ---------------- */
int main()
{
    int sizes[] = {10000, 50000, 100000, 500000, 1000000};
    int num_sizes = sizeof(sizes) / sizeof(int);

    srand(0);

    printf("n,seq,recursive,posix,omp_for,omp_reduction\n");

    for (int s = 0; s < num_sizes; s++) {
        int Nloc = sizes[s];
        a = malloc(Nloc * sizeof(double));

        for (int i = 0; i < Nloc; i++)
            a[i] = 0.999 + (double)rand() / RAND_MAX * 0.002;

        double t, seq_t, rec_t, posix_t, ompfor_t, ompred_t;

        t = omp_get_wtime();
        product_seq(0, Nloc);
        seq_t = omp_get_wtime() - t;

        t = omp_get_wtime();
        product_recursive(0, Nloc, MAX_DEPTH);
        rec_t = omp_get_wtime() - t;

        thread_data d = {0, Nloc, MAX_DEPTH, 1.0};
        t = omp_get_wtime();
        thread_func(&d);
        posix_t = omp_get_wtime() - t;

        t = omp_get_wtime();
        product_omp_for();
        ompfor_t = omp_get_wtime() - t;

        t = omp_get_wtime();
        product_omp_reduction();
        ompred_t = omp_get_wtime() - t;

        printf("%d,%.6f,%.6f,%.6f,%.6f,%.6f\n",
               Nloc, seq_t, rec_t, posix_t, ompfor_t, ompred_t);

        free(a);
    }

    return 0;
}