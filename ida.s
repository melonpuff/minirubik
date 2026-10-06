#---------------.data---------------
.data 
inputs:         .string "12345671111111" # test 1: solve case 15bytes
                .string "23745612123332" # test 2: 3 steps case
                .string "21345671111111" # test 3: 11 steps case
face_table:     .byte 0, 0, 0, 1, 1, 1, 2, 2, 2
turns_table:    .byte 1, 2, 3, 1, 2, 3, 1, 2, 3
face_char:      .byte 82, 66, 68 #'R', 'B', 'D'

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

#---------------.text---------------
.text
main: 
    la s0, inputs
    li s1, NUM_INPUTS
main_loop:
    # validate
    mv a0, s0
    jal ra, validate
    li  a7, 1            # ecall 11: print char in a0
    ecall
    li  a0, 10           # '\n' 
    li  a7, 11           # ecall 11: print char in a0
    ecall
    # encode
    mv a0, s0 # put inputs[i] back to a0
    jal ra, encode
    # solve_ida_star
    # a0 now get o_rank, a1 now get p_rank
    addi s2, a0, 0 # save o_rank to s2 (caller save)
    addi s3, a1, 0 # save p_rank to s3 (caller save)
    
    jal ra, solve_ida_star



    # next iteration 
    addi s0, s0, 15 # next input[i]
    addi s1, s1 -1  # NUM_INPUTS --
    bne s1, x0, main_loop


    

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
    addi s4, ra, 0 # s4: store ra of solve_ida_star
    addi s5, a0, 0 # s5: store o_rank
    addi s6, a1, 0 # s6: store p_rank
    jal ra, get_heuristic
    # a0: initial_h
    # if a0 = 0 solved
    beq a0, x0, main
    addi t0, a0, 0 # t0: bound
    addi t1, x0, 0 # t1: depth
solve_ida_star_loop:
    addi t2, t1, 0 # t2: address offset of stack
    la t
    


    


get_heuristic:  # a0: o_rank, a1: p_rank
    la t0, ori_heuristic 
    add t0, t0, a0
    lbu t1, 0(t0) # t1: ori_heuristic[o_rank]
    la t0, perm_heuristic
    add t0, t0, a1 
    lbu t2, 0(t0) # t2: perm_heuristic[p_rank]
    begu t3, t1, t2
    addi a0, t2, 0
    bne t3, x0, get_heuristic_done # t1>t2 ori_h > perm_h
    addi a0, t1, 0
get_heuristic_done: 
    jalr x0, ra, 0

    