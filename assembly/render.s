

.data
# Faces in net order U L F R B D. Top-left pixel of each face on the net:
# one face above, four across, one below; 2 x 2 facelets of 4 x 3 pixels each,
# 1 pixel between faces.
render_face_x0:     .byte 9, 0, 9, 18, 27, 9 # face_x0[6]
render_face_y0:     .byte 0, 7, 7, 7, 7, 14  # face_y0[6]

# slot_facelet[pos][s]: facelet id (face * 4 + row * 2 + col) showing slot s
# of corner position pos; slots clockwise from the U/D facelet.
# Positions: 0 FUL (fixed), 1 FUR, 2 FDR, 3 FDL, 4 BUR, 5 BDR, 6 BDL, 7 BUL.
render_slot_facelet:
                    .byte 2, 8, 5       # 0 FUL: U F L
                    .byte 3, 12, 9      # 1 FUR: U R F
                    .byte 21, 11, 14    # 2 FDR: D F R
                    .byte 20, 7, 10     # 3 FDL: D L F
                    .byte 1, 16, 13     # 4 BUR: U B R
                    .byte 23, 15, 18    # 5 BDR: D R B
                    .byte 22, 19, 6     # 6 BDL: D B L
                    .byte 0, 4, 17      # 7 BUL: U L B

# One quarter turn of face f (0 R, 1 B, 2 D) on positions 0..7: the cubie at
# position i comes from position source[f][i] and gains twist[f][i].
# solver.c's source[][] and twist[][] with the fixed position 0 added in front.
render_source:      .byte 0, 2, 5, 3, 1, 4, 6, 7      # R
                    .byte 0, 1, 2, 3, 5, 6, 7, 4      # B
                    .byte 0, 1, 3, 6, 4, 2, 5, 7      # D

render_twist:       .byte 0, 1, 2, 0, 2, 1, 0, 0      # R
                    .byte 0, 0, 0, 0, 1, 2, 1, 2      # B
                    .byte 0, 0, 0, 0, 0, 0, 0, 0      # D

# What sits at positions 0..7 (cubie number and twist); position 0 never moves.
render_cubie:       .zero 8
render_twist_now:   .zero 8
render_cubie_next:  .zero 8              # scratch for one quarter turn
render_twist_next:  .zero 8

                    .align 4
# 24-bit RGB color of each face: U white, L orange, F green, R red, B blue,
# D yellow.
render_palette:     .word 0xFFFFFF, 0xFF8000, 0x00C000, 0xFF0000, 0x0000FF, 0xFFFF00

.equ RENDER_DELAY, 200                 # busy-wait between frames (GUI only)

.text
#---------------------------------------------------------------
# render_solution: a0 = input string, a1 = solution length
# Draws the input state, then applies path[0 .. a1-1] one move at a time and
# redraws after each, so the animation follows the solver's actual output.
render_solution:
    addi sp, sp, -16
    sw   ra, 12(sp)
    sw   s7, 8(sp)
    sw   s8, 4(sp)
    addi s8, a1, 0             # s8: solution length
    jal  ra, render_load       # a0 is still the input string
    jal  ra, render_draw
    addi s7, x0, 0             # s7: i
render_solution_loop:
    bge  s7, s8, render_solution_done
    la   t0, path
    add  t0, t0, s7
    lbu  a0, 0(t0)             # a0: move = path[i]
    jal  ra, render_apply
    jal  ra, render_delay
    jal  ra, render_draw
    addi s7, s7, 1             # i++
    jal  x0, render_solution_loop
render_solution_done:
    lw   ra, 12(sp)
    lw   s7, 8(sp)
    lw   s8, 4(sp)
    addi sp, sp, 16
    jalr x0, ra, 0

#---------------------------------------------------------------
# render_load: a0 = 14-digit input string (already validated)
# Fills render_cubie and render_twist_now: digit '1'..'7' is cubie 1..7,
# digit '1'..'3' is twist 0..2; position 0 holds cubie 0 with twist 0.
render_load:
    la   t0, render_cubie
    la   t1, render_twist_now
    sb   x0, 0(t0)             # position 0: cubie 0
    sb   x0, 0(t1)             #             twist 0
    addi t2, x0, 0             # t2: i
render_load_loop:
    add  t3, a0, t2            # a0: &input[]
    lbu  t4, 0(t3)             # load digit input[i]
    addi t4, t4, -48           # '1'..'7' -> 1..7
    lbu  t5, 7(t3)             # load input[i + 7]
    addi t5, t5, -49           # '1'..'3' -> 0..2
    add  t3, t0, t2            # t3: &cubie[i]
    sb   t4, 1(t3)             # store cubie[i]
    add  t3, t1, t2            # load twist[i]
    sb   t5, 1(t3)             # store twist[i]
    addi t2, t2, 1             # i ++
    addi t3, x0, 7
    blt  t2, t3, render_load_loop
    jalr x0, ra, 0

#---------------------------------------------------------------

# render_apply: a0 = move (0..8)
# Applies turns_table[move] quarter turns of face_table[move] to the
# positions, the same rule as solver.c's quarter_turn.
render_apply:
    la   t0, face_table
    add  t0, t0, a0
    lbu  a1, 0(t0)             # a1: face
    la   t0, turns_table
    add  t0, t0, a0
    lbu  a2, 0(t0)             # a2: quarter turns left
    slli t0, a1, 3             # face * 8: start of this face's table rows
    la   a3, render_source
    add  a3, a3, t0            # a3: &render_source[face][0]
    la   a4, render_twist
    add  a4, a4, t0            # a4: &render_twist[face][0]
render_apply_turn:
    addi t0, x0, 0             # t0: i
render_apply_pos:              # next[i] comes from position source[i]
    add  t1, a3, t0
    lbu  t1, 0(t1)             # t1: from = source[face][i]
    la   t2, render_cubie
    add  t2, t2, t1
    lbu  t3, 0(t2)             # t3: cubie[from]
    la   t2, render_twist_now
    add  t2, t2, t1
    lbu  t4, 0(t2)             # t4: twist[from]
    add  t1, a4, t0
    lbu  t1, 0(t1)
    add  t4, t4, t1            # + twist[face][i], at most 4
    addi t1, x0, 3
    blt  t4, t1, render_apply_mod_done
    addi t4, t4, -3            # mod 3
render_apply_mod_done:
    la   t2, render_cubie_next
    add  t2, t2, t0
    sb   t3, 0(t2)
    la   t2, render_twist_next
    add  t2, t2, t0
    sb   t4, 0(t2)
    addi t0, t0, 1
    addi t1, x0, 8
    blt  t0, t1, render_apply_pos
    addi t0, x0, 0             # copy next back into the current arrays
render_apply_copy:
    la   t1, render_cubie_next
    add  t1, t1, t0
    lbu  t3, 0(t1)
    la   t1, render_cubie
    add  t1, t1, t0
    sb   t3, 0(t1)
    la   t1, render_twist_next
    add  t1, t1, t0
    lbu  t3, 0(t1)
    la   t1, render_twist_now
    add  t1, t1, t0
    sb   t3, 0(t1)
    addi t0, t0, 1
    addi t1, x0, 8
    blt  t0, t1, render_apply_copy
    addi a2, a2, -1
    bne  a2, x0, render_apply_turn
    jalr x0, ra, 0

#---------------------------------------------------------------
# render_draw: paints the 24 facelets for the current positions.
# The LED matrix is row-major, one word per LED: pixel (x, y) is at
# LED_MATRIX_0_BASE + (y * LED_MATRIX_0_WIDTH + x) * 4.
render_draw:
    addi a0, x0, 0             # a0: pos
    addi a1, x0, 0             # a1: index = pos * 3 + s into slot_facelet
render_draw_pos:
    la   t0, render_cubie
    add  t0, t0, a0
    lbu  a2, 0(t0)             # a2: cubie[pos]
    la   t0, render_twist_now
    add  t0, t0, a0
    lbu  a3, 0(t0)             # a3: twist[pos]
    addi a4, x0, 0             # a4: s
render_draw_slot:
    # color: the cubie's home sticker k = (s + twist) mod 3 shows face
    # slot_facelet[cubie][k] / 4
    add  t0, a4, a3            # s + twist, at most 4
    addi t1, x0, 3
    blt  t0, t1, render_draw_k_done
    addi t0, t0, -3
render_draw_k_done:
    slli t1, a2, 1
    add  t1, t1, a2            # cubie * 3
    add  t1, t1, t0            # + k
    la   t2, render_slot_facelet
    add  t2, t2, t1
    lbu  t2, 0(t2)             # home facelet of that sticker
    srli t2, t2, 2             # / 4 = its face = its color
    slli t2, t2, 2             # x 4 bytes per palette word
    la   t3, render_palette
    add  t3, t3, t2
    lw   a5, 0(t3)             # a5: RGB of this sticker
    # place: facelet id = slot_facelet[pos][s] -> face, row, col
    la   t0, render_slot_facelet
    add  t0, t0, a1
    lbu  t0, 0(t0)             # t0: facelet id
    srli t1, t0, 2             # t1: face
    andi t2, t0, 1             # t2: col
    srli t3, t0, 1
    andi t3, t3, 1             # t3: row
    la   t4, render_face_x0
    add  t4, t4, t1
    lbu  t4, 0(t4)
    slli t2, t2, 2
    add  t4, t4, t2            # t4: x0 = face_x0 + 4 * col
    la   t5, render_face_y0
    add  t5, t5, t1
    lbu  t5, 0(t5)
    slli t2, t3, 1
    add  t2, t2, t3            # 3 * row
    add  t5, t5, t2            # t5: y0 = face_y0 + 3 * row
    # address = BASE + (y0 * WIDTH + x0) * 4, y0 * WIDTH by repeated addition
    addi t6, t4, 0             # t6: y0 * WIDTH + x0
    li   t2, LED_MATRIX_0_WIDTH
render_draw_row_offset:
    beq  t5, x0, render_draw_row_done
    add  t6, t6, t2
    addi t5, t5, -1
    jal  x0, render_draw_row_offset
render_draw_row_done:
    slli t6, t6, 2
    li   t0, LED_MATRIX_0_BASE
    add  t6, t0, t6            # t6: address of the facelet's top-left LED
    slli t2, t2, 2             # t2: bytes per LED row
    addi t3, x0, 3             # t3: rows left
render_draw_fill:              # one facelet: 3 rows of 4 LEDs
    sw   a5, 0(t6)
    sw   a5, 4(t6)
    sw   a5, 8(t6)
    sw   a5, 12(t6)
    add  t6, t6, t2
    addi t3, t3, -1
    bne  t3, x0, render_draw_fill
    addi a1, a1, 1             # next slot_facelet index
    addi a4, a4, 1             # s++
    addi t0, x0, 3
    blt  a4, t0, render_draw_slot
    addi a0, a0, 1             # pos++
    addi t0, x0, 8
    blt  a0, t0, render_draw_pos
    jalr x0, ra, 0

#---------------------------------------------------------------
# render_delay: busy-waits so each frame stays visible in the GUI.
render_delay:
    li   t0, RENDER_DELAY
render_delay_loop:
    addi t0, t0, -1
    bne  t0, x0, render_delay_loop
    jalr x0, ra, 0
