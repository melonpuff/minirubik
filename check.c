/* Host-side checks for the IDA* solver in ida.c, measured against the exact
 * BFS table that solver.c builds:
 *
 *   H1  get_heuristic never exceeds the exact distance, over all states.
 *   H2  every table the search depends on is fully populated, with its
 *       maximum value and its solved entry verified.
 *   H3  solve_ida_star returns, for every state, a solution that works and
 *       whose length equals the exact distance.
 *   H4  every packed accessor agrees with its unpacked table at both even and
 *       odd indices.
 *
 * usage: ida_check [STRIDE]   H3 visits every STRIDE-th state (default 1).
 */
#define main solver_main
#include "solver.c"
#undef main

enum { DIAMETER = 11, PERM_H_MAX = 7, ORI_H_MAX = 6 };

/* Defined in tables.c and ida.c; ida.c is compiled with main renamed. */
extern const uint8_t perm_heuristic[PERMUTATIONS];
extern const uint8_t ori_heuristic[ORIENTATIONS];
extern const uint16_t perm_transition_flat[3 * 8192];
extern const uint16_t ori_transition_flat[3 * 1024];
uint8_t get_heuristic(uint16_t p_rank, uint16_t o_rank);
int solve_ida_star(uint16_t p_rank, uint16_t o_rank, uint8_t path[DIAMETER]);

/* Number of states at each distance from solved, as listed in report.md. */
static const uint32_t level_counts[DIAMETER + 1] = {
    1, 9, 54, 321, 1847, 9992, 50136, 227536, 870072, 1887748, 623800, 2644,
};

static uint8_t distance[STATES];
static unsigned failures;

static void fail(const char *what, uint32_t index, long got, long want)
{
    if (failures++ < 10)
        printf("    FAIL %s at %lu: got %ld, want %ld\n", what,
               (unsigned long) index, got, want);
}

/* A whole-table property, such as its maximum, has no single index to blame. */
static void fail_table(const char *table, const char *what, long got, long want)
{
    if (failures++ < 10)
        printf("    FAIL %s %s: got %ld, want %ld\n", table, what, got, want);
}

static int report(const char *name, unsigned before, const char *detail)
{
    int ok = failures == before;
    printf("%s %s  %s\n", name, ok ? "PASS" : "FAIL", detail);
    return ok;
}

/* Exact distance of every state: follow solver's table, which stores one move
 * toward solved, until reaching a state whose distance is already known.
 */
static void exact_distances(const uint8_t *toward_solved)
{
    memset(distance, UINT8_MAX, STATES);
    distance[0] = 0;
    for (uint32_t rank = 1; rank < STATES; ++rank) {
        uint32_t chain[DIAMETER + 1], here = rank;
        int length = 0;
        state_t state;
        while (distance[here] == UINT8_MAX) {
            if (length > DIAMETER || toward_solved[here] >= MOVES) {
                fail("BFS table walk", rank, length, DIAMETER);
                return;
            }
            chain[length++] = here;
            unrank_state(here, &state);
            state = apply_move(state, toward_solved[here]);
            here = rank_state(&state);
        }
        while (length > 0) {
            --length;
            distance[chain[length]] = (uint8_t) (distance[here] + 1);
            here = chain[length];
        }
    }
}

/* The reference: solver's BFS reached all states, the walk gives every state
 * a distance, solved is 0, the maximum is the diameter, and every level holds
 * the published number of states.
 */
static void check_bfs_table(const uint8_t *toward_solved, uint8_t diameter)
{
    uint32_t count[UINT8_MAX + 1] = {0};
    uint8_t max = 0;
    if (!toward_solved) {
        fail_table("BFS table", "states reached", 0, STATES);
        return;
    }
    exact_distances(toward_solved);
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        ++count[distance[rank]];
        if (distance[rank] > max)
            max = distance[rank];
    }
    if (count[UINT8_MAX])
        fail_table("BFS table", "states without a distance",
                   (long) count[UINT8_MAX], 0);
    if (distance[0] != 0)
        fail_table("BFS table", "solved entry", distance[0], 0);
    if (max != DIAMETER || diameter != DIAMETER)
        fail_table("BFS table", "maximum", max, DIAMETER);
    for (uint8_t d = 0; d <= DIAMETER; ++d)
        if (count[d] != level_counts[d])
            fail("BFS table: states at distance", d, (long) count[d],
                 (long) level_counts[d]);
}

/* Each valid row entry must be the rank solver.c gets from a real quarter
 * turn, so the table is fully populated, its maximum is size - 1, and the
 * solved entry is right along with every other.
 */
static void check_transition(const char *name, const uint16_t *table,
                             uint16_t stride, uint16_t size, int perm)
{
    uint16_t max = 0;
    for (uint8_t face = 0; face < 3; ++face)
        for (uint16_t rank = 0; rank < size; ++rank) {
            state_t state;
            unrank_state(perm ? (uint32_t) rank * ORIENTATIONS : rank, &state);
            state = quarter_turn(state, face);
            uint32_t full = rank_state(&state);
            uint16_t want = (uint16_t) (perm ? full / ORIENTATIONS
                                             : full % ORIENTATIONS);
            uint16_t got = table[face * stride + rank];
            if (got != want)
                fail(name, (uint32_t) face * stride + rank, got, want);
            if (got > max)
                max = got;
        }
    if (max != size - 1)
        fail_table(name, "maximum", max, size - 1);
}

/* A table is the exact distance to rank 0 in its own graph exactly when it is
 * populated, 0 at solved and only there, and every other entry is one more
 * than its nearest neighbour. That pins every entry, so the maximum is then
 * compared against the value the tables are known to reach.
 */
static void check_heuristic(const char *name, const uint8_t *h,
                            const uint16_t *transition, uint16_t stride,
                            uint16_t size, uint8_t want_max)
{
    uint8_t max = 0;
    if (h[0] != 0)
        fail_table(name, "solved entry", h[0], 0);
    for (uint16_t rank = 0; rank < size; ++rank) {
        if (h[rank] > DIAMETER) {
            fail(name, rank, h[rank], DIAMETER);
            continue;
        }
        if (h[rank] > max)
            max = h[rank];
        if (rank == 0)
            continue;
        uint8_t nearest = UINT8_MAX;
        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next = rank;
            for (uint8_t turn = 0; turn < 3; ++turn) {
                next = transition[face * stride + next];
                if (h[next] < nearest)
                    nearest = h[next];
            }
        }
        if (h[rank] != nearest + 1)
            fail(name, rank, h[rank], nearest + 1);
    }
    if (max != want_max)
        fail_table(name, "maximum", max, want_max);
}

/* H4 registry. ida.c reads every table unpacked today; a packed accessor
 * added later is listed here with the unpacked table it replaces.
 */
typedef struct {
    const char *name;
    uint8_t (*get)(uint16_t index);
    const uint8_t *unpacked;
    uint16_t count;
} packed_t;

static const packed_t packed[] = {
    /* {"perm_heuristic", get_perm_heuristic, perm_heuristic, PERMUTATIONS}, */
    {NULL, NULL, NULL, 0},
};

int main(int argc, char **argv)
{
    uint32_t stride = 1;
    uint8_t diameter = 0;
    char detail[160];
    int ok = 1;
    if (argc > 2 || (argc == 2 && (stride = (uint32_t) atol(argv[1])) == 0)) {
        fputs("usage: ida_check [STRIDE]\n", stderr);
        return 2;
    }

    /* H2 first: H1 and H3 measure against the BFS table it validates. */
    unsigned before = failures;
    uint8_t *toward_solved = build_table(&diameter);
    check_bfs_table(toward_solved, diameter);
    check_transition("perm_transition_flat", perm_transition_flat, 8192,
                     PERMUTATIONS, 1);
    check_transition("ori_transition_flat", ori_transition_flat, 1024,
                     ORIENTATIONS, 0);
    check_heuristic("perm_heuristic", perm_heuristic, perm_transition_flat,
                    8192, PERMUTATIONS, PERM_H_MAX);
    check_heuristic("ori_heuristic", ori_heuristic, ori_transition_flat, 1024,
                    ORIENTATIONS, ORI_H_MAX);
    ok &= report("H2", before,
                 "BFS table, 2 transition tables, 2 heuristic tables");
    free(toward_solved);
    if (!ok) {
        puts("H1, H3 skipped: they need a valid BFS table");
        return 1;
    }

    before = failures;
    uint32_t tight = 0;
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        uint8_t h = get_heuristic((uint16_t) (rank / ORIENTATIONS),
                                  (uint16_t) (rank % ORIENTATIONS));
        if (h > distance[rank])
            fail("H1 heuristic exceeds distance", rank, h, distance[rank]);
        tight += h == distance[rank];
    }
    snprintf(detail, sizeof detail,
             "h <= d over %d states; h == d for %lu of them", STATES,
             (unsigned long) tight);
    ok &= report("H1", before, detail);

    before = failures;
    uint32_t checked = 0;
    for (uint32_t rank = 0; rank < STATES; rank += stride) {
        uint8_t path[DIAMETER];
        state_t state;
        int length = solve_ida_star((uint16_t) (rank / ORIENTATIONS),
                                    (uint16_t) (rank % ORIENTATIONS), path);
        ++checked;
        if (checked % 100000 == 0)
            fprintf(stderr, "  H3 %lu / %lu\r", (unsigned long) rank,
                    (unsigned long) STATES);
        if (length != distance[rank]) {
            fail("H3 solution length", rank, length, distance[rank]);
            continue;
        }
        unrank_state(rank, &state);
        for (int i = 0; i < length; ++i)
            state = apply_move(state, path[i]);
        if (rank_state(&state) != 0)
            fail("H3 solution does not solve", rank, (long) rank_state(&state),
                 0);
    }
    snprintf(detail, sizeof detail,
             "%lu states%s, each solved in exactly d moves",
             (unsigned long) checked, stride == 1 ? " (all)" : " (sampled)");
    ok &= report("H3", before, detail);

    before = failures;
    unsigned accessors = 0;
    for (const packed_t *a = packed; a->name; ++a, ++accessors)
        for (uint16_t i = 0; i < a->count; ++i)
            if (a->get(i) != a->unpacked[i])
                fail(i % 2 ? "H4 packed, odd index" : "H4 packed, even index",
                     i, a->get(i), a->unpacked[i]);
    snprintf(detail, sizeof detail, "%u packed accessor(s)%s", accessors,
             accessors ? ", every even and odd index"
                       : "; ida.c reads every table unpacked");
    ok &= report("H4", before, detail);

    return !ok || output_failed();
}
