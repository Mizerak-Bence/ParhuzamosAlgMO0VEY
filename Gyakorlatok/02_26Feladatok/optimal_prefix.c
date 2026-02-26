#include <stdio.h>
#include <math.h>

#define MAXP 128
#define MAXT 32

int main(void)
{
    int n;
    printf("Add meg a bemenet meretet (n): ");
    scanf("%d", &n);

    double logn = log2(n);
    int Tp = (int)ceil(logn);
    int p  = (int)ceil(n / logn);

    /* Aktivitas matrix */
    char activity[MAXT][MAXP];

    /* inicializalas */
    for (int t = 0; t < Tp; t++)
        for (int i = 0; i < p; i++)
            activity[t][i] = '.';

    /*
       OPTIMAL_PREFIX fazisok:
       1) blokkon beluli prefix: Tp/2 ido
       2) blokk prefix: Tp/2 ido
       3) korrekcio: 1 ido
    */

    int t = 0;

    /* 1. fazis */
    for (; t < Tp / 2; t++)
        for (int i = 0; i < p; i++)
            activity[t][i] = '#';

    /* 2. fazis */
    for (; t < Tp - 1; t++)
        for (int i = 0; i < p; i++)
            activity[t][i] = '#';

    /* 3. fazis (korrekcio) */
    for (int i = 0; i < p; i++)
        activity[Tp - 1][i] = '#';

    /* ---- KIMENET TERMINALRA ---- */

    printf("\nOPTIMAL_PREFIX vizsgalat (n = %d)\n", n);
    printf("Processzorok szama p = %d\n", p);
    printf("Parhuzamos ido Tp = %d\n\n", Tp);

    printf("Ido/Proc ");
    for (int i = 0; i < p; i++)
        printf("P%-2d ", i);
    printf("\n");

    for (int i = 0; i < 8 + 4 * p; i++)
        printf("-");
    printf("\n");

    for (int ti = 0; ti < Tp; ti++) {
        printf("%6d  ", ti);
        for (int i = 0; i < p; i++)
            printf(" %c  ", activity[ti][i]);
        printf("\n");
    }

    /* ---- FAJLBA KIIRAS ---- */

    FILE *f = fopen("optimal_prefix_activity.txt", "w");
    fprintf(f, "Ido/Proc ");
    for (int i = 0; i < p; i++)
        fprintf(f, "P%-2d ", i);
    fprintf(f, "\n");

    for (int ti = 0; ti < Tp; ti++) {
        fprintf(f, "%6d  ", ti);
        for (int i = 0; i < p; i++)
            fprintf(f, " %c  ", activity[ti][i]);
        fprintf(f, "\n");
    }
    fclose(f);

    /* ---- MEROSZAMOK ---- */

    double W = n - 1;
    double C = p * Tp;
    double S = W / Tp;
    double E = S / p;

    printf("\nMeresi ertekek:\n");
    printf("Munka (W)       = %.0f\n", W);
    printf("Koltseg (C)     = %.0f\n", C);
    printf("Gyorsitas (S)   = %.2f\n", S);
    printf("Hatekonysag (E) = %.3f\n", E);

    printf("\nAktivitasi matrix fajlba mentve: optimal_prefix_activity.txt\n");

    return 0;
}