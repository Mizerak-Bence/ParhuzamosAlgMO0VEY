#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double ms_since(clock_t start, clock_t end) {
    return (double)(end - start) * 1000.0 / (double)CLOCKS_PER_SEC;
}

static void usage(const char *exe) {
    printf("Hasznalat:\n");
    printf("  %s gen <fajl_kimenet> <karakter_db>\n", exe);
    printf("  %s measure <csv_kimenet> <fajl_bemenet> <probak_szama>\n", exe);
    printf("measure CSV oszlopok: trial;iter_ms;rec_ms;lines;empty_lines\n");
}

static void gen_file(const char *path, int chars) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Nem tudom megnyitni: '%s'\n", path);
        exit(1);
    }

    const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 \n";
    int m = (int)strlen(alphabet);
    for (int i = 0; i < chars; ++i) {
        char c = alphabet[rand() % m];
        fwrite(&c, 1, 1, f);
    }

    fclose(f);
    printf("Kesz: %s (%d karakter)\n", path, chars);
}

static int is_empty_line(const char *line) {
    return (line[0] == '\n' || line[0] == '\0');
}

static int count_lines_iter(FILE *f, int *empty_lines) {
    char buf[4096];
    int lines = 0;
    int empty = 0;
    while (fgets(buf, (int)sizeof(buf), f)) {
        lines++;
        if (is_empty_line(buf)) empty++;
    }
    *empty_lines = empty;
    return lines;
}

static int count_lines_rec_impl(FILE *f, int *empty_lines, char *buf, size_t buf_size) {
    if (!fgets(buf, (int)buf_size, f)) return 0;
    if (is_empty_line(buf)) (*empty_lines)++;
    return 1 + count_lines_rec_impl(f, empty_lines, buf, buf_size);
}

static int count_lines_rec(FILE *f, int *empty_lines) {
    char buf[4096];
    return count_lines_rec_impl(f, empty_lines, buf, sizeof(buf));
}

static void measure(const char *csv_out, const char *file_in, int trials) {
    FILE *csv = fopen(csv_out, "w");
    if (!csv) {
        fprintf(stderr, "Nem tudom megnyitni: '%s'\n", csv_out);
        exit(1);
    }
    fprintf(csv, "trial;iter_ms;rec_ms;lines;empty_lines\n");

    for (int t = 0; t < trials; ++t) {
        int empty_it = 0;
        FILE *f = fopen(file_in, "rb");
        if (!f) {
            fprintf(stderr, "Nem tudom megnyitni: '%s'\n", file_in);
            exit(1);
        }
        clock_t t0 = clock();
        int lines_it = count_lines_iter(f, &empty_it);
        clock_t t1 = clock();
        double it_ms = ms_since(t0, t1);
        fclose(f);

        int empty_rc = 0;
        f = fopen(file_in, "rb");
        if (!f) {
            fprintf(stderr, "Nem tudom megnyitni: '%s'\n", file_in);
            exit(1);
        }
        t0 = clock();
        int lines_rc = count_lines_rec(f, &empty_rc);
        t1 = clock();
        double rc_ms = ms_since(t0, t1);
        fclose(f);


        (void)lines_rc;

        fprintf(csv, "%d;%.6f;%.6f;%d;%d\n", t, it_ms, rc_ms, lines_it, empty_it);
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

    if (strcmp(argv[1], "gen") == 0) {
        if (argc != 4) {
            usage(argv[0]);
            return 1;
        }
        int chars = atoi(argv[3]);
        if (chars < 0) {
            fprintf(stderr, "Hibas karakter_db\n");
            return 1;
        }
        gen_file(argv[2], chars);
        return 0;
    }

    if (strcmp(argv[1], "measure") == 0) {
        if (argc != 5) {
            usage(argv[0]);
            return 1;
        }
        int trials = atoi(argv[4]);
        if (trials <= 0) {
            fprintf(stderr, "Hibas probak_szama\n");
            return 1;
        }
        measure(argv[2], argv[3], trials);
        return 0;
    }

    usage(argv[0]);
    return 1;
}
