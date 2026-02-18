#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Hiba: Pontosan ket egesz szamot kell megadni argumentumkent!\n");
        printf("Hasznalat: %s also_hatar felso_hatar\n", argv[0]);
        return 1;
    }

    int also = atoi(argv[1]);
    int felso = atoi(argv[2]);

    if (also > felso) {
        printf("Hiba: Az also hatar nem lehet nagyobb, mint a felso!\n");
        return 1;
    }

    srand(time(NULL));

    int veletlen = also + rand() % (felso - also + 1);

    printf("Veletlen szam [%d, %d] intervallumban: %d\n", also, felso, veletlen);

    return 0;
}
