#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
        int8_t depth = 0; // Index of the top node; also its distance from the start
        
        stack[depth].p_rank = initial_p_rank;
        stack[depth].o_rank = initial_o_rank;
        stack[depth].g = 0;
        stack[depth].last_face = -1;
        stack[depth].move_index = 0;

        while (depth >= 0) {
            uint8_t h = get_heuristic(stack[depth].p_rank, stack[depth].o_rank);

            // Pruning: Path cost + estimated remaining cost > bound, prune this path
            if (stack[depth].g + h > bound) {
                depth--; 
                continue;
            }

            if (h == 0) { // Goal found
                // Level k left through move_index - 1, since move_index is
                // advanced before the child is pushed
                for (int8_t k = 0; k < depth; k++)
                    path[k] = stack[k].move_index - 1;
                return depth;
            }

            if (stack[depth].move_index < 9) {
                uint8_t face = face_table[stack[depth].move_index]; // 0 or 1 or 2
                uint8_t turns = turns_table[stack[depth].move_index]; // 1 or 2 or 3: how many turns 
                
                if (face == stack[depth].last_face) {
                    stack[depth].move_index += 3;
                    continue;
                }
                stack[depth].move_index++;
                uint16_t next_p = stack[depth].p_rank;
                uint16_t next_o = stack[depth].o_rank;
                // Look up tphe table iteratively based on the number of quarter turns (1, 2, or 3)
                // Uses explicit bitwise shifts (<< 13 and << 10) instead of implicit multiplication
                for (uint8_t i = 0; i < turns; i++) {
                    next_p = perm_transition_flat[(face << 13) + next_p];
                    next_o = ori_transition_flat[(face << 10) + next_o];
                }
                depth++; 
                stack[depth].p_rank = next_p;
                stack[depth].o_rank = next_o;
                stack[depth].g = stack[depth - 1].g + 1; // child is one move deeper
                stack[depth].last_face = face;
                stack[depth].move_index = 0; 
            }
            else
                depth--; // All 9 moves exhausted, backtrack to previous level

        }
    }
    return -1;
}


// Host driver: same CLI and exit status as solver (0 solved, 1 failure,
// 2 usage), so its output can be compared against ./solver directly

static const char *const move_names[9] = {"R",  "R2", "R'", "B", "B2",
                                          "B'", "D",  "D2", "D'"};

// Check PPPPPPPOOOOOOO in one pass, as ida.s does: cubie digits 1..7 with no
// repeats, orientation digits 1..3, exactly 14 characters, and an orientation
// sum divisible by 3. Reads input[14], so input must hold at least 15 bytes.
// Returns 0 if the state is valid, 1 otherwise.
static int validate(const char *input) {
    uint8_t seen = 0, sum = 0;
    if (input[14] != '\0')
        return 1;
    for (int i = 0; i < 7; i++) {
        // Unsigned subtraction folds the lower bound into the upper one
        uint8_t p = (uint8_t) (input[i] - '1');
        if (p >= 7)
            return 1;
        uint8_t mask = (uint8_t) (1U << p);
        if (seen & mask)
            return 1; // Duplicate cubie
        seen |= mask;
        uint8_t o = (uint8_t) (input[i + 7] - '1');
        if (o >= 3)
            return 1;
        sum += o;
    }
    while (sum >= 3) // sum % 3 without a remainder instruction
        sum -= 3;
    return sum != 0; // Twist parity
}


static uint16_t encode(const char *input, uint16_t *o_rank) {
    uint16_t p_rank = 0, o = 0;
    for (int i = 0; i < 6; i++)
        o = (uint16_t) ((o << 1) + o + (input[i + 7] - '1')); // o * 3 + digit
    // i = 6 adds nothing: no cubie to its right, and the multiplier is 1
    for (int i = 0; i < 6; i++) {
        uint16_t smaller = 0;
        // '1'..'7' sort like 1..7, so the characters compare directly
        for (int j = i + 1; j < 7; j++)
            smaller += input[j] < input[i];
        
        uint16_t product = 0, x = p_rank;
        // this for loop replace product = p_rank * (7 - i) 
        // simulates multiplication, shift and add 
        for (uint8_t k = (uint8_t) (7 - i); k > 0; k >>= 1, x <<= 1)
            if (k & 1)
                product += x;
        p_rank = (uint16_t) (product + smaller);
    }
    *o_rank = o;
    return p_rank;
}

int main(int argc, char **argv) {
    uint16_t p_rank, o_rank;
    uint8_t path[MAX_DEPTH];
    // validate reads input[14]; checking the length first keeps that in bounds
    if (argc != 2 || strlen(argv[1]) != 14 || validate(argv[1])) {
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n",
                argc > 0 && argv[0] ? argv[0] : "ida");
        return 2;
    }
    p_rank = encode(argv[1], &o_rank);
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
