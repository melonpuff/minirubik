#include <stdint.h>
#include <stdio.h>

#define MAX_DEPTH 11

// Heuristic Tables 
// Stores the minimum number of moves to reach the solved state
extern const uint8_t perm_heuristic[5040];
extern const uint8_t ori_heuristic[729];

// Transition Tables (Flattened and padded to powers of 2)
// Stores the next state's rank after a turn.
// Could be perm_transition[3][2^13], ori_transition[3][2^10], instead
// 5040 is padded to 8192 (2^13) and 729 is padded to 1024 (2^10) 
extern const uint16_t perm_transition_flat[3 * 8192];
extern const uint16_t ori_transition_flat[3 * 1024];

// Lookup tables to eliminate division and modulo for move generation
const uint8_t face_table[9]  = {0, 0, 0, 1, 1, 1, 2, 2, 2};
const uint8_t turns_table[9] = {1, 2, 3, 1, 2, 3, 1, 2, 3};

// StackNode discards physical arrays, storing only the two integer ranks
typedef struct {
    uint16_t p_rank; 
    uint16_t o_rank;
    uint8_t g; //cumulated steps
    int8_t last_face;
    uint8_t move_index;
} StackNode; 

// Look up the admissible heuristic value
uint8_t get_heuristic(uint16_t p_rank, uint16_t o_rank) {
    uint8_t p_h = perm_heuristic[p_rank];
    uint8_t o_h = ori_heuristic[o_rank];
    return (p_h > o_h) ? p_h : o_h; 
}

// Main search: Pass the initial scrambled state ranks
// Writes the solution as move indices (0..8, same order as face_table) into
// path and returns its length, or -1 if no solution fits within MAX_DEPTH
int solve_ida_star(uint16_t initial_p_rank, uint16_t initial_o_rank,
                   uint8_t path[MAX_DEPTH]) {
    uint8_t initial_h = get_heuristic(initial_p_rank, initial_o_rank);
    if (initial_h == 0) return 0; // Already solved, reach solve state

    StackNode stack[MAX_DEPTH + 1]; 
    
    // Outer loop: Gradually relax the depth bound
    for (uint8_t bound = initial_h; bound <= MAX_DEPTH; bound++) {
        int8_t sp = 0; 
        
        stack[sp].p_rank = initial_p_rank;
        stack[sp].o_rank = initial_o_rank;
        stack[sp].g = 0;
        stack[sp].last_face = -1;
        stack[sp].move_index = 0;

        while (sp >= 0) {
            uint16_t curr_p = stack[sp].p_rank;
            uint16_t curr_o = stack[sp].o_rank;
            uint8_t g = stack[sp].g;
            int8_t last_face = stack[sp].last_face;
            uint8_t move_idx = stack[sp].move_index;

            uint8_t h = get_heuristic(curr_p, curr_o);

            // Pruning: Path cost + estimated remaining cost > bound, prune this path
            if (g + h > bound) {
                sp--; 
                continue;
            }

            if (h == 0) { // Goal found
                // Level k left through move_index - 1, since move_index is
                // advanced before the child is pushed
                for (int8_t k = 0; k < sp; k++)
                    path[k] = stack[k].move_index - 1;
                return sp;
            }

            if (move_idx < 9) { // going down 
                stack[sp].move_index++; 
                
                uint8_t face = face_table[move_idx];
                uint8_t turns = turns_table[move_idx];
                
                if (face == last_face) continue; 

                // State transition via flattened table lookup
                uint16_t next_p = curr_p;
                uint16_t next_o = curr_o;
                
                // Look up the table iteratively based on the number of quarter turns (1, 2, or 3)
                // Uses explicit bitwise shifts (<< 13 and << 10) instead of implicit multiplication
                for (uint8_t i = 0; i < turns; i++) {
                    next_p = perm_transition_flat[(face << 13) + next_p];
                    next_o = ori_transition_flat[(face << 10) + next_o];
                }

                sp++; 
                stack[sp].p_rank = next_p;
                stack[sp].o_rank = next_o;
                stack[sp].g = g + 1;
                stack[sp].last_face = face;
                stack[sp].move_index = 0; 
            } else {
                sp--; // All 9 moves exhausted, backtrack to previous level
            }
        }
    }
    return -1;
}


// Host driver: same CLI and exit status as solver (0 solved, 1 failure,
// 2 usage), so its output can be compared against ./solver directly

static const char *const move_names[9] = {"R",  "R2", "R'", "B", "B2",
                                          "B'", "D",  "D2", "D'"};

// Parse PPPPPPPOOOOOOO into ranks: Lehmer code of the permutation and base-3
// value of the first six orientations, matching rank_state in solver.c
static int parse_ranks(const char *input, uint16_t *p_rank, uint16_t *o_rank) {
    uint8_t p[7];
    uint8_t seen = 0, sum = 0;
    for (int i = 0; i < 14; i++) {
        int limit = i < 7 ? 7 : 3;
        if (input[i] < '1' || input[i] > '0' + limit)
            return 0;
    }
    if (input[14] != '\0')
        return 0;
    for (int i = 0; i < 7; i++) {
        p[i] = (uint8_t) (input[i] - '1');
        if (seen >> p[i] & 1)
            return 0; // Duplicate cubie
        seen |= (uint8_t) (1U << p[i]);
        sum += (uint8_t) (input[i + 7] - '1');
    }
    if (sum % 3)
        return 0; // Twist parity

    // encode to rank 
    *p_rank = 0;
    *o_rank = 0;
    // 1. Lehmer code encode p_rank
    for (int i = 0; i < 7; i++) {
        uint8_t smaller = 0;
        for (int j = i + 1; j < 7; j++)
            smaller += p[j] < p[i];
        *p_rank = (uint16_t) (*p_rank * (7 - i) + smaller);
    }
    // 2. Base-3-interger encode o_rank
    for (int i = 0; i < 6; i++)
        *o_rank = (uint16_t) (*o_rank * 3 + (input[i + 7] - '1'));
    return 1;
}

int main(int argc, char **argv) {
    uint16_t p_rank, o_rank;
    uint8_t path[MAX_DEPTH];
    if (argc != 2 || !parse_ranks(argv[1], &p_rank, &o_rank)) {
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n",
                argc > 0 && argv[0] ? argv[0] : "ida");
        return 2;
    }
    int length = solve_ida_star(p_rank, o_rank, path);
    if (length < 0) {
        fputs("no solution within 11 moves; tables are wrong\n", stderr);
        return 1;
    }
    for (int i = 0; i < length; i++)
        printf("%s%s", i ? " " : "", move_names[path[i]]);
    putchar('\n');
    // stdout is fully buffered off a terminal; write errors surface here
    return fflush(stdout) != 0 || ferror(stdout);
}
