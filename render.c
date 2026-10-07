/* Host model of render.s, called from ida.c when built with -DRENDER: prints
 * the input state as the 35 x 20 LED picture, then applies the solution in
 * path[] one move at a time and prints the picture after each move. One
 * character per LED; '.' is a separator pixel. Same tables and steps as
 * render.s, so its frames can be compared against the Ripes LED matrix.
 */
#include <stdint.h>
#include <stdio.h>

/* From ida.c: move m turns face face_table[m] by turns_table[m] quarters. */
extern const uint8_t face_table[9];
extern const uint8_t turns_table[9];

enum { WIDTH = 35, HEIGHT = 20 };

/* Faces in net order U L F R B D; the letter stands for the face's color. */
static const char face_letter[6] = {'W', 'O', 'G', 'R', 'B', 'Y'};

/* Top-left pixel of each face: one face above, four across, one below.
 * A face is 2 x 2 facelets of 4 x 3 pixels, with 1 pixel between faces.
 */
static const int face_x0[6] = {9, 0, 9, 18, 27, 9};
static const int face_y0[6] = {0, 7, 7, 7, 7, 14};

/* Facelet id = face * 4 + row * 2 + col, each face seen from outside.
 * slot_facelet[pos][s]: the facelet showing slot s of corner position pos,
 * slots ordered clockwise (seen from outside) starting at the U/D facelet.
 * Positions: 0 FUL (fixed), 1 FUR, 2 FDR, 3 FDL, 4 BUR, 5 BDR, 6 BDL, 7 BUL.
 */

// 8 corner positions x 3 sticker slots = 24 facelets; each entry is a facelet id
static const int slot_facelet[8][3] = {
    {2, 8, 5},    /* 0 FUL: U F L */
    {3, 12, 9},   /* 1 FUR: U R F */
    {21, 11, 14}, /* 2 FDR: D F R */
    {20, 7, 10},  /* 3 FDL: D L F */
    {1, 16, 13},  /* 4 BUR: U B R */
    {23, 15, 18}, /* 5 BDR: D R B */
    {22, 19, 6},  /* 6 BDL: D B L */
    {0, 4, 17},   /* 7 BUL: U L B */
};

/* One quarter turn of face f (0 R, 1 B, 2 D) on positions 0..7: the cubie at
 * position i comes from position source[f][i] and gains twist[f][i].
 * solver.c's source[][] and twist[][] with the fixed position 0 added in front.
 */
static const int source[3][8] = {
    {0, 2, 5, 3, 1, 4, 6, 7}, /* R */
    {0, 1, 2, 3, 5, 6, 7, 4}, /* B */
    {0, 1, 3, 6, 4, 2, 5, 7}, /* D */
};
static const int twist_add[3][8] = {
    {0, 1, 2, 0, 2, 1, 0, 0}, /* R */
    {0, 0, 0, 0, 1, 2, 1, 2}, /* B */
    {0, 0, 0, 0, 0, 0, 0, 0}, /* D */
};

static int cubie[8], twist[8]; /* what sits at positions 0..7; 0 never moves */

/* render_load: digit '1'..'7' is cubie 1..7, digit '1'..'3' is twist 0..2. */
static void render_load(const char *input)
{
    cubie[0] = 0;
    twist[0] = 0;
    for (int i = 0; i < 7; i++) {
        cubie[i + 1] = input[i] - '0';
        twist[i + 1] = input[i + 7] - '1';
    }
}

/* render_apply: turns_table[move] quarter turns of face_table[move]. */
static void render_apply(int move)
{
    int f = face_table[move];
    for (int turn = 0; turn < turns_table[move]; turn++) {
        int next_cubie[8], next_twist[8];
        for (int i = 0; i < 8; i++) {
            next_cubie[i] = cubie[source[f][i]];
            next_twist[i] = (twist[source[f][i]] + twist_add[f][i]) % 3;
        }
        for (int i = 0; i < 8; i++) {
            cubie[i] = next_cubie[i];
            twist[i] = next_twist[i];
        }
    }
}

/* render_draw: prints the current positions as the LED picture. */
static void render_draw(void)
{
    char pixel[HEIGHT][WIDTH];

    // slot_facelet translates between two ways of naming the 24 facelets:
    // 1st (its index): [pos][slot], 8 corner positions x 3 sticker slots.
    //     The loop below walks the facelets in this order.
    // 2nd (what it stores): facelet id = 4 * face_id + 2 * row_id + col_id,
    //     which gives the face and the cell, and so the pixel position.
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            pixel[y][x] = '.';
    for (int pos = 0; pos < 8; pos++)
        for (int s = 0; s < 3; s++) {
            /* Where this sticker is drawn, and which home sticker it shows:
             * the cubie's own slot (s + twist) mod 3, whose face is id / 4.
             */
            int id = slot_facelet[pos][s];
            int face = slot_facelet[cubie[pos]][(s + twist[pos]) % 3] / 4;
            // id / 4: which face? 0-5: U, L, F, R, B, D
            int x0 = face_x0[id / 4] + 4 * (id % 2);
            int y0 = face_y0[id / 4] + 3 * (id % 4 / 2);
            for (int y = y0; y < y0 + 3; y++)
                for (int x = x0; x < x0 + 4; x++)
                    pixel[y][x] = face_letter[face];
        }
    for (int y = 0; y < HEIGHT; y++)
        printf("%.*s\n", WIDTH, pixel[y]);
}

/* render_solution: draw the input, then redraw after each move in path. */
void render_solution(const char *input, const uint8_t *path, int length)
{
    render_load(input);
    render_draw();
    for (int i = 0; i < length; i++) {
        render_apply(path[i]);
        putchar('\n');
        render_draw();
    }
}
