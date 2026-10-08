/* Host-side generator for the four lookup tables that ida.c reads.
 *
 * Prints them as RISC-V assembly in .data for Ripes, or with --c as C arrays
 * for building ida on the host. It runs once at build time, so it is written
 * for clarity rather than speed or memory. It carries its own copy of the cube
 * model, which must stay identical to source[][] and twist[][] in solver.c.
 */
#include <stdio.h>
#include <string.h>

enum {
    PERMS = 5040,       /* 7! arrangements of the seven moving corners */
    ORIS = 729,         /* 3^6 orientations; the seventh follows from parity */
    PERM_STRIDE = 8192, /* rows padded to powers of two, so ida.c reaches a */
    ORI_STRIDE = 1024,  /* face's row with a shift instead of a multiply */
    UNVISITED = 255
};

/* A quarter turn of face f (0 = R, 1 = B, 2 = D) moves the cubie at position
 * source[f][i] to position i and adds twist[f][i] to its orientation.
 */
static const int source[3][7] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const int twist[3][7] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

static int perm_transition[3 * PERM_STRIDE];
static int ori_transition[3 * ORI_STRIDE];
static int perm_heuristic[PERMS];
static int ori_heuristic[ORIS];

static int factorial(int n)
{
    return n <= 1 ? 1 : n * factorial(n - 1);
}

/* Lehmer code: for each position, count the later cubies with a smaller
 * number, and read those counts as a factorial-base numeral.
 */
static int perm_rank(const int p[7])
{
    int rank = 0;
    for (int i = 0; i < 7; i++) {
        int smaller = 0;
        for (int j = i + 1; j < 7; j++)
            if (p[j] < p[i])
                smaller++;
        rank = rank * (7 - i) + smaller;
    }
    return rank;
}

/* Inverse of perm_rank: digit i selects the digit-th smallest unused cubie. */
static void perm_unrank(int rank, int p[7])
{
    int used[7] = {0};
    for (int i = 0; i < 7; i++) {
        int digit = rank / factorial(6 - i);
        rank %= factorial(6 - i);
        for (int cubie = 0; cubie < 7; cubie++) {
            if (used[cubie])
                continue;
            if (digit == 0) {
                p[i] = cubie;
                used[cubie] = 1;
                break;
            }
            digit--;
        }
    }
}

/* The first six orientations read as a base-3 numeral. */
static int ori_rank(const int o[7])
{
    int rank = 0;
    for (int i = 0; i < 6; i++)
        rank = rank * 3 + o[i];
    return rank;
}

/* Inverse of ori_rank; the seventh orientation makes the sum divisible by 3. */
static void ori_unrank(int rank, int o[7])
{
    int sum = 0;
    for (int i = 5; i >= 0; i--) {
        o[i] = rank % 3;
        rank /= 3;
        sum += o[i];
    }
    o[6] = (3 - sum % 3) % 3;
}

/* Fewest moves from each rank back to rank 0, by breadth-first search from
 * rank 0: the FIFO queue hands out ranks in order of distance, so the first
 * time a rank is reached is along a shortest path. Quarter, half and inverse
 * turns all count as one move. Returns 0 if some rank is never reached.
 */
//distances(perm_transition, PERM_STRIDE, PERMS, perm_heuristic)
//distances(ori_transition, ORI_STRIDE, ORIS, ori_heuristic)
static int distances(const int *transition, int stride, int size,
                     int *distance)
{
    int queue[size]; /* each rank enters the queue exactly once */
    int head = 0, tail = 0;
    for (int i = 0; i < size; i++)
        distance[i] = UNVISITED;
    distance[0] = 0;
    queue[tail] = 0;
    tail ++;
    // here and there are ranks
    while (head < tail) {
        int here = queue[head++];
        for (int face = 0; face < 3; face++) {
            int there = here;
            for (int turn = 0; turn < 3; turn++) {
                there = transition[face * stride + there];
                if (distance[there] == UNVISITED) {
                    distance[there] = distance[here] + 1;
                    queue[tail++] = there;
                }
            }
        }
    }
    return tail == size;
}

static void emit(const char *name, const int *table, int count, int half,
                 int c)
{
    if (c)
        printf("\nconst %s %s[%d] = {", half ? "uint16_t" : "uint8_t", name,
               count);
    else
        printf("\n    .globl %s\n    .align 2\n%s:", name, name);
    for (int i = 0; i < count; i++) {
        if (i % 16 == 0)
            printf("\n    %s", c ? "" : half ? ".half " : ".byte ");
        else
            printf(c ? " " : ", ");
        printf(c ? "%d," : "%d", table[i]);
    }
    puts(c ? "\n};" : "");
}

int main(int argc, char **argv)
{
    int c = argc == 2 && strcmp(argv[1], "--c") == 0;
    if (argc > 2 || (argc == 2 && !c)) {
        fputs("usage: gen_tables [--c]\n", stderr);
        return 2;
    }

    /* Moves act on the arrangement and the orientations independently, so
     * each gets its own table of quarter-turn results.
     */
    for (int rank = 0; rank < PERMS; rank++) {
        int p[7], next[7];
        perm_unrank(rank, p);
        for (int face = 0; face < 3; face++) {
            for (int i = 0; i < 7; i++)
                next[i] = p[source[face][i]];
                // source[face][i] previous location
                // p[source[face][i]] which cubie on preious location
            perm_transition[face * PERM_STRIDE + rank] = perm_rank(next);
        }
    }
    for (int rank = 0; rank < ORIS; rank++) {
        int o[7], next[7];
        ori_unrank(rank, o);
        for (int face = 0; face < 3; face++) {
            for (int i = 0; i < 7; i++)
                next[i] = (o[source[face][i]] + twist[face][i]) % 3; 
                // source[face][i] previous location
                // o[source[face][i]] orientation of preious location
                // twist[face][i]  add orientation value
            ori_transition[face * ORI_STRIDE + rank] = ori_rank(next);
        }
    }

    if (!distances(perm_transition, PERM_STRIDE, PERMS, perm_heuristic) ||
        !distances(ori_transition, ORI_STRIDE, ORIS, ori_heuristic)) {
        fputs("could not reach every permutation and orientation\n", stderr);
        return 1;
    }

    if (c) {
        puts("/* Generated by gen_tables --c; do not edit. */");
        puts("#include <stdint.h>");
    } else {
        puts("# Generated by gen_tables; do not edit.");
        puts("# perm_transition_flat[face << 13 | p], "
             "ori_transition_flat[face << 10 | o]");
        /* Ripes rejects ".section .rodata"; .data assembles there. */
        puts("    .data");
    }
    emit("perm_heuristic", perm_heuristic, PERMS, 0, c);
    emit("ori_heuristic", ori_heuristic, ORIS, 0, c);
    emit("perm_transition_flat", perm_transition, 3 * PERM_STRIDE, 1, c);
    emit("ori_transition_flat", ori_transition, 3 * ORI_STRIDE, 1, c);

    /* stdout is fully buffered off a terminal; write errors surface here. */
    return fflush(stdout) != 0 || ferror(stdout);
}
