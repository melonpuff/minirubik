#---------------.data---------------
.data 
inputs:         .string "12345671111111" # test 1: solve case 15bytes
                .string "23745612123332" # test 2: 3 steps case
                .string "21345671111111" # test 3: 11 steps case
face_table:     .byte 0, 0, 0, 1, 1, 1, 2, 2, 2
turns_table:    .byte 1, 2, 3, 1, 2, 3, 1, 2, 3
face_char:      .byte 82, 66, 68 #'R', 'B', 'D'
expected:       .byte 0, 3, 11 #(steps)
invalid_msg:    .string "invalid input\n"

path:           .zero 11 # uint8_t path[11]
                .align 4 # align memory addr
stack:          .zero 96 # StackNode stack[11 + 1]; 
                        # choose StackNode size as 8 bytes 
                        # instead of 7 bytes   

# .equ: equate 
.equ MAX_DEPTH,  11
.equ NODE_SIZE,  8      
.equ INPUT_LEN,  15     
.equ NUM_INPUTS, 3
.equ NUM_MOVES,  9
.equ INVALID,    255

#---------------.text---------------
.text
main:
    la   s0, inputs            # s0: address of the current input string
    addi s1, x0, NUM_INPUTS    # s1: inputs left
    addi s4, x0, 0             # s4: number of failed checks (exit code)
    la   s5, expected          # s5: &expected[i]
main_loop:
    addi a0, s0, 0
    jal  ra, validate          # a0 = 0 valid, 1 invalid
    bne  a0, x0, main_invalid  # skip encode and search for an invalid input
    addi a0, s0, 0             # validate overwrote a0; pass the string again
    jal  ra, encode            # a0 = o_rank, a1 = p_rank
    addi s2, a0, 0             # keep o_rank across the calls
    addi s3, a1, 0             # keep p_rank across the calls
    jal  ra, solve_ida_star    # a0 = solution length (-1 = not found)
    addi s6, a0, 0             # s6: keep the length across the calls
    jal  ra, print_path        # print path[0 .. a0-1]
    # ---- T5: replaying the path from the start state must reach solved ----
    addi a0, s2, 0             # o_rank of the input
    addi a1, s3, 0             # p_rank of the input
    addi a2, s6, 0             # solution length
    jal  ra, verify_path       # a0 = 0 reaches solved, 1 does not
    add  s4, s4, a0
    # ---- T6: the length must equal the known optimal length ----
    lbu  t0, 0(s5)             # t0: expected length of this input
    beq  s6, t0, main_next
    # T6 failed
    addi s4, s4, 1
    jal  x0, main_next
main_invalid:                  # print invalid_msg one character at a time
    la   t0, invalid_msg       # (ecall 4 would also print the trailing '\0')
main_invalid_loop:
    lbu  a0, 0(t0)
    beq  a0, x0, main_invalid_done  # stop at '\0'
    addi a7, x0, 11            # ecall 11: print the character in a0
    ecall
    addi t0, t0, 1
    jal  x0, main_invalid_loop
main_invalid_done:
    lbu  t0, 0(s5)             # rejecting is correct only if expected says so
    addi t1, x0, INVALID
    beq  t0, t1, main_next
    addi s4, s4, 1
main_next:
    addi s0, s0, INPUT_LEN     # next input string
    addi s5, s5, 1             # next expected length
    addi s1, s1, -1
    bne  s1, x0, main_loop
    addi a0, s4, 0             # exit code = number of failed checks (0 = all pass)
    addi a7, x0, 93            # ecall 93: exit with the code in a0
    ecall

# validate(p[i])
validate:
    addi t0, x0, 0 # t0: i
    addi t1, x0, 0 # t1: seen
    addi t2, x0, 0 # t2: sum
    #  if (input[14] != '\0')
    lbu t3, 14(a0)
    bne t3, x0, validate_bad

validate_loop:
    add t3, a0, t0 # t3: &p[i]
    lbu t4, 0(t3) # t4: p[i]
    addi t4, t4, -49 # - '1' to get value
    # check 1 (front 7 char): less than 7
    sltiu t5, t4, 7 # if t < 7 t5 = 1
    beq t5, x0, validate_bad
    # check 2 (front 7 char): is unique
    addi t5, x0, 1 # t5: mask
    sll t5, t5, t4 # 1 << p[i]
    and t6, t1, t5
    bne t6, x0, validate_bad
    or t1, t1, t5 # update seen
    # check 3 (rear 7 char): less than 3
    lbu t4, 7(t3) # t4: p[i + 7]
    addi t4, t4, -49 # - '1' to get value
    sltiu t5, t4, 3 # if t4 < 3 t5 = 1
    beq t5, x0, validate_bad
    # check 4 (rear 7 char): sum rear here, check later
    add t2, t2, t4 # add p[i + 7] to sum
    addi t0, t0, 1 # increment i
    li t5, 7
    blt t0, t5, validate_loop
    li t5, 3 # t5: 3

validate_mod3:
    bltu t2, t5, validate_done # branch if sum < 3
    addi t2, t2, -3 # sum += -3
    j validate_mod3

validate_done:
    bne t2, x0, validate_bad
    li a0, 0
    jalr x0, ra, 0

validate_bad:
    li a0, 1
    jalr x0, ra, 0


encode: 
    addi a1, x0, 0 # a1(return values): p_rank
    addi t1, x0, 0 # t1: o_rank
    addi t2, x0, 0 # t2: i

encode_loop_orank:
    slli t3, t1, 1 # t3: stores o_rank << 1
    add t1, t3, t1 # + (o_rank << 1) + o_rank
    addi t3, t2, 7 # t3: i + 7
    add t4, a0, t3 # t4: &inputs[i + 7]
    lbu t5, 0(t4) # t5: inputs[i + 7]
    addi t5, t5, -49 # inputs[i + 7] - '1'
    add t1, t1, t5 # + (o_rank << 1) + o_rank + inputs[i + 7] - '1'
    addi t2, t2, 1
    addi t3, x0, 6
    bltu t2, t3, encode_loop_orank

    addi t2, x0, 0 # reset i for p_rank loop

encode_loop_prank:
    addi t3, x0, 0 # t3: smaller
    addi t4, t2, 1 # t4: j = i + 1

    add t5, a0, t2 # t5: &inputs[i]
    lbu t0, 0(t5) # t0 : inputs[i]
    
encode_loop_prank_inloop1: # find smaller
    add t5, a0, t4 # t5: &inputs[j]
    lbu t6, 0(t5) # t6 : inputs[j]
    sltu t5, t6, t0 # input[j] < input[i];
    add t3, t3, t5 # smaller += input[j] < input[i];

    addi t4, t4, 1
    addi t5, x0, 7
    bltu t4, t5, encode_loop_prank_inloop1

    addi t0, x0, 0 # t0: product
    addi t4, x0, 7
    sub t4, t4, t2 # t4: k
    add t5, x0, a1 # t5: x = p_rank,

encode_loop_prank_inloop2: # calculate p_rank
    andi t6, t4, 1
    beq t6, x0, skip_add
    add t0, t0, t5
skip_add: 
    srli t4, t4, 1
    slli t5, t5, 1
    bne t4, x0, encode_loop_prank_inloop2
    add a1, t0, t3

    addi t2, t2, 1 # increment i (encode_loop_prank)
    addi t4, x0, 6
    bltu t2, t4, encode_loop_prank

    addi a0, t1, 0 # move o_rank to a0
    jalr x0, ra, 0 # jump back to main_loop 
    
solve_ida_star: # a0: o_rank, a1: p_rank
    addi sp, sp, -32
    sw   ra, 28(sp)
    sw   s4, 24(sp)
    sw   s5, 20(sp)
    sw   s6, 16(sp)
    sw   s7, 12(sp)
    sw   s8, 8(sp)
    sw   s9, 4(sp)

    
    addi s5, a0, 0 # s5: store o_rank
    addi s6, a1, 0 # s6: store p_rank
    la   s4, stack
    jal  ra, get_heuristic
    # a0: initial_h
    addi s7, a0, 0 # s7: bound
    # return if solved 
    bne  a0, x0, solve_bound_loop
    addi a0, x0, 0 # return 0
    jal x0, solve_return 
    
solve_bound_loop: # for loop
    addi s8, x0, 0          # s8: depth  
    sh   s6, 0(s4)          # stack[0].p_rank
    sh   s5, 2(s4)          # stack[0].o_rank
    sb   x0, 4(s4)          # g = 0
    addi t0, x0, -1
    sb   t0, 5(s4)          # last_face = -1
    sb   x0, 6(s4)          # move_index = 0
solve_depth_loop: # while loop
    blt  s8, x0, solve_next_bound
    slli s9, s8, 3 # every StackNode in stack is 8 bytes
    add s9, s9, s4
    lhu a1, 0(s9) # load stack[depth].p_rank
    lhu a0, 2(s9) # load stack[depth].o_rank
    jal ra, get_heuristic
    lbu t1, 4(s9) # load stack[depth].g
    add t1, t1, a0 # stack[depth].g + h
    bge s7, t1, solve_not_pruned
    addi s8, s8, -1 # depth --
    jal  x0, solve_depth_loop
solve_not_pruned: 
    bne a0, x0, solve_not_found
    la   t0, path                    # t0: &path[k]
    addi t1, s4, 0                   # t1: &stack[k]
    addi t2, x0, 0                   # t2: k
   
solve_path_loop:
    bge  t2, s8, solve_path_done     # k < depth
    lbu  t3, 6(t1)                   # t3: stack[k].move_index
    addi t3, t3, -1                  # move_index - 1
    sb   t3, 0(t0)                   # path[k]
    addi t0, t0, 1                   # &path + 1 byte
    addi t1, t1, 8                   # next node in stack
    addi t2, t2, 1                   # k++
    jal  x0, solve_path_loop

solve_path_done:
    addi a0, s8, 0                   # return depth
    jal  x0, solve_return

solve_not_found:  
    lbu  t0, 6(s9) # t0: stack[depth].move_index
    addi t1, x0, NUM_MOVES
    bge  t0, t1, solve_backtrack     # move_index >= 9: all 9 moves tried
    la   t1, face_table
    add  t1, t1, t0 # &face_table[stack[depth].move_index]
    lbu  t2, 0(t1)  # t2: face: face_table[stack[depth].move_index]
    la   t1, turns_table    
    add  t1, t1, t0 # &turns_table[stack[depth].move_index]
    lbu  t3, 0(t1)  # t3: turns = turns_table[stack[depth].move_index]
    lb   t4, 5(s9) # t4: stack[depth].last_face
    bne  t2, t4, solve_new_move
    addi t0, t0, 3 # stack[depth].move_index += 3;
    sb   t0, 6(s9)
    jal  x0, solve_not_found

solve_new_move:
    addi t0, t0, 1 # stack[depth].move_index++;
    sb   t0, 6(s9) 
    lhu  t5, 0(s9) # t5: next_p = stack[depth].p_rank
    lhu  t6, 2(s9) # t6: next_o = stack[depth].o_rank

solve_turn_loop:
    slli t1, t2, 13  # face << 13
    add  t1, t1, t5  # (face << 13) + next_p
    slli t1, t1, 1   # x 2: each entry is 2 bytes
    la   t0, perm_transition_flat
    add  t1, t0, t1
    lhu  t5, 0(t1)   # next_p = perm_transition_flat[...]
    slli t1, t2, 10  # face << 10
    add  t1, t1, t6  # (face << 10) + next_o
    slli t1, t1, 1
    la   t0, ori_transition_flat
    add  t1, t0, t1
    lhu  t6, 0(t1)   # next_o = ori_transition_flat[...]
    addi t3, t3, -1
    bne  t3, x0, solve_turn_loop

   #---- push the child into stack[depth + 1] = s9 + 8 ----
   # s9 is the addr of parent node
    addi s8, s8, 1   # depth++
    sh   t5, 8(s9)   # child.p_rank
    sh   t6, 10(s9)  # child.o_rank
    lbu  t0, 4(s9)   # parent's g
    addi t0, t0, 1  # # parent's g + 1
    sb   t0, 12(s9)  # child.g = parent.g + 1
    sb   t2, 13(s9)  # child.last_face = face
    sb   x0, 14(s9)  # child.move_index = 0
    jal x0, solve_depth_loop
solve_backtrack:
    addi s8, s8, -1                  # depth--
    jal  x0, solve_depth_loop

solve_next_bound: 
    addi s7, s7, 1 # bound ++
    addi t0, x0, MAX_DEPTH
    bge t0, s7, solve_bound_loop
    addi a0, x0, -1 # return -1 (no found) -> solve_return

solve_return:
    lw   ra, 28(sp)
    lw   s4, 24(sp)
    lw   s5, 20(sp)
    lw   s6, 16(sp)
    lw   s7, 12(sp)
    lw   s8, 8(sp)
    lw   s9, 4(sp)
    addi sp, sp, 32
    jalr x0, ra, 0


get_heuristic:  # a0: o_rank, a1: p_rank
    la t0, ori_heuristic 
    add t0, t0, a0
    lbu t1, 0(t0) # t1: ori_heuristic[o_rank]
    la t0, perm_heuristic
    add t0, t0, a1 
    lbu t2, 0(t0) # t2: perm_heuristic[p_rank]
    addi a0, t1, 0
    bgeu t1, t2, get_heuristic_done # t1>t2 ori_h > perm_h
    addi a0, t2, 0
get_heuristic_done: 
    jalr x0, ra, 0


print_path:                    # a0 = length -> prints path as "R B' D2 ...", then '\n'
    addi t1, a0, 0             # t1: length (a0 is reused for printing)
    addi t0, x0, 0             # t0: i
    la   t2, path              # t2: &path[0]
print_path_loop:
    bge  t0, t1, print_path_done
    beq  t0, x0, print_path_move    # no space before the first move
    addi a0, x0, 32            # ' '
    addi a7, x0, 11            # ecall 11: print the character in a0
    ecall
print_path_move:
    add  t3, t2, t0
    lbu  t3, 0(t3)             # t3: move = path[i] (0..8)
    la   t4, face_table
    add  t4, t4, t3
    lbu  t4, 0(t4)             # t4: face (0, 1, 2)
    la   t5, face_char
    add  t5, t5, t4
    lbu  a0, 0(t5)             # 'R', 'B' or 'D'
    addi a7, x0, 11
    ecall
    la   t4, turns_table
    add  t4, t4, t3
    lbu  t4, 0(t4)             # t4: turns (1, 2, 3)
    addi t5, x0, 2
    beq  t4, t5, print_path_half
    addi t5, x0, 3
    beq  t4, t5, print_path_prime
    jal  x0, print_path_next   # turns = 1: no suffix
print_path_half:
    addi a0, x0, 50            # '2'
    addi a7, x0, 11
    ecall
    jal  x0, print_path_next
print_path_prime:
    addi a0, x0, 39            # '\''
    addi a7, x0, 11
    ecall
print_path_next:
    addi t0, t0, 1             # i++
    jal  x0, print_path_loop
print_path_done:
    addi a0, x0, 10            # '\n'
    addi a7, x0, 11
    ecall
    jalr x0, ra, 0

# verify for T5, follow steps again
verify_path:                   # a0 = o_rank, a1 = p_rank, a2 = length
                               # -> a0 = 0 if path[0 .. length-1] takes the state to solved, else 1
    addi t0, x0, 0             # t0: i
    la   t1, path              # t1: &path[0]
    addi t5, a1, 0             # t5: p
    addi t6, a0, 0             # t6: o
verify_move_loop:
    bge  t0, a2, verify_check
    add  t2, t1, t0
    lbu  t2, 0(t2)             # t2: move = path[i]
    la   t3, face_table
    add  t3, t3, t2
    lbu  t3, 0(t3)             # t3: face
    la   t4, turns_table
    add  t4, t4, t2
    lbu  t4, 0(t4)             # t4: turns
verify_turn_loop:              # one quarter turn of face t3, turns times
    slli t2, t3, 13            # face << 13
    add  t2, t2, t5            # + p
    slli t2, t2, 1             # x 2: each entry is 2 bytes
    la   a3, perm_transition_flat
    add  t2, a3, t2
    lhu  t5, 0(t2)             # p = perm_transition_flat[...]
    slli t2, t3, 10            # face << 10
    add  t2, t2, t6            # + o
    slli t2, t2, 1
    la   a3, ori_transition_flat
    add  t2, a3, t2
    lhu  t6, 0(t2)             # o = ori_transition_flat[...]
    addi t4, t4, -1
    bne  t4, x0, verify_turn_loop
    addi t0, t0, 1             # i++
    jal  x0, verify_move_loop
verify_check:
    or   t2, t5, t6            # zero only when p == 0 and o == 0
    sltu a0, x0, t2            # a0 = (t2 != 0): 0 = solved, 1 = not solved
    jalr x0, ra, 0
