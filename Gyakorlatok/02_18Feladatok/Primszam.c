#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

int prim_e(int n) {
    if (n < 2) return 0;
    if (n == 2) return 1;
    if (n % 2 == 0) return 0;

    for (int i = 3; i <= sqrt(n); i += 2) {
        if (n % i == 0) return 0;
    }
    return 1;
}

int prim_db(int n) {
    int db = 0;
    for (int i = 1; i <= n; i++) {
        if (prim_e(i)) db++;
    }
    return db;
}

int main() {
    FILE *f = fopen("eredmenyek.csv", "w");
    if (!f) {
        printf("Nem sikerult megnyitni a fajlt!\n");
        return 1;
    }

    // fejléc – pontosvessző, hogy Excel jól értse
    fprintf(f, "n;futasi_ido_ms;prim_db\n");

    for (int n = 1000; n <= 20000; n += 1000) {
        clock_t start = clock();
        int db = prim_db(n);
        clock_t end = clock();

        double ido_ms = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;

        fprintf(f, "%d;%.3f;%d\n", n, ido_ms, db);
    }

    fclose(f);
    printf("Kesz! Az eredmenyek az eredmenyek.csv fajlban.\n");
    return 0;
}
