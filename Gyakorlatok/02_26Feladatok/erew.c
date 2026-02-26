#include <stdio.h>
#include <math.h>

void print_activity(int n)
{
    int p = n;
    int steps = (int)log2(n);

    printf("\nEREW_PREFIX aktivitasi matrix (n = %d)\n", n);
    printf("Ido/PE ");
    for (int i = 0; i < p; i++)
        printf("%2d ", i);
    printf("\n");

    for (int i = 0; i < 6 + 3 * p; i++)
        printf("-");
    printf("\n");

    for (int t = 0; t < steps; t++) {
        int active = n / (1 << (t + 1));

        printf("%6d ", t);
        for (int i = 0; i < p; i++) {
            if (i < active)
                printf(" # ");
            else
                printf(" . ");
        }
        printf("\n");
    }
}

void analyze(int n)
{
    double T1 = n - 1;
    double Tp = log2(n);
    double W  = n - 1;
    double C  = n * Tp;
    double S  = T1 / Tp;
    double E  = S / n;

    print_activity(n);

    printf("\nMeresi eredmenyek:\n");
    printf("Munka (W)        = %.0f\n", W);
    printf("Parhuzamos ido   = %.0f\n", Tp);
    printf("Koltseg (C)      = %.0f\n", C);
    printf("Gyorsitas (S)    = %.2f\n", S);
    printf("Hatekonysag (E)  = %.4f\n", E);
}

int main(void)
{
    printf("OSSZEFOGLALO TABLAZAT\n");
    printf("n    W     Tp    C      S       E\n");
    printf("-------------------------------------\n");

    for (int n = 2; n <= 32; n *= 2) {
        double Tp = log2(n);
        double W  = n - 1;
        double C  = n * Tp;
        double S  = W / Tp;
        double E  = S / n;

        printf("%-4d %-5.0f %-5.0f %-6.0f %-7.2f %-6.3f\n",
               n, W, Tp, C, S, E);
    }

    analyze(8);   // részletes vizsgálat egy n-re

    return 0;
}