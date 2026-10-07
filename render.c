/* Host prototype of the LED renderer planned for ida.s: reads one 14-digit
 * state and prints the 35 x 20 LED picture that render.s will draw for it,
 * one character per LED ('.' is a separator pixel).
 *
 *   render PPPPPPPOOOOOOO
 */
#include <stdio.h>
#include <string.h>

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

int main(int argc, char **argv)
{
    int cubie[8], twist[8]; /* what sits at positions 0..7; 0 never moves */
    char pixel[HEIGHT][WIDTH];

    if (argc != 2 || strlen(argv[1]) != 14) {
        fputs("usage: render PPPPPPPOOOOOOO\n", stderr);
        return 2;
    }
    cubie[0] = 0;
    twist[0] = 0;
    for (int i = 0; i < 7; i++) {
        cubie[i + 1] = argv[1][i] - '0';     /* digit '1'..'7' = cubie 1..7 */
        twist[i + 1] = argv[1][i + 7] - '1'; /* digit '1'..'3' = twist 0..2 */
        if (cubie[i + 1] < 1 || cubie[i + 1] > 7 || twist[i + 1] < 0 ||
            twist[i + 1] > 2) {
            fputs("usage: render PPPPPPPOOOOOOO\n", stderr);
            return 2;
        }
    }

    // slot_facelet translates between two ways of naming the 24 facelets:
    // 1st (its index): [pos][slot], 8 corner positions x 3 sticker slots.
    //     The loop below walks the facelets in this order.
    // 2nd (what it stores): facelet id = 4 * face_id + 2 * row_id + col_id,
    //     which gives the face and the cell, and so the pixel position.
    memset(pixel, '.', sizeof pixel);
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
    return 0;
}
