#include <stdio.h>
#include <stdlib.h>

static int next_id = 0;

static int emit_call(FILE* out, int l, int r) {
    int id = next_id++;
    fprintf(out, "  n%d [label=\"[%d..%d]\"];\n", id, l, r);

    if (l < r) {
        int m = (l + r) / 2;
        int left = emit_call(out, l, m);
        int right = emit_call(out, m + 1, r);
        fprintf(out, "  n%d -> n%d;\n", id, left);
        fprintf(out, "  n%d -> n%d;\n", id, right);
    }

    return id;
}

int main(int argc, char** argv) {
    int n = 8;
    if (argc >= 2) {
        n = atoi(argv[1]);
        if (n <= 0) n = 8;
    }

    fprintf(stdout, "digraph CREW_PREFIX_CALLS {\n");
    fprintf(stdout, "  node [shape=box];\n");
    next_id = 0;
    emit_call(stdout, 1, n);
    fprintf(stdout, "}\n");
    return 0;
}
