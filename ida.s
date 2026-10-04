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
    mv a0, s0
    jal ra, validate


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
    jalr x0, 0(ra)

validate_bad:
    li a0, 1
    jalr x0, 0(ra)



