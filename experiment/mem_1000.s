.text

main:
        li t0, 1000       # Set loop counter to N = 10,000,000 
        li t1, 0x20000000     # Set starting memory address
        li t2, 0xFFFFFFFF           # data to be written
loop:
        sw t2, 0(t1)          
        addi t1, t1, 1        # Increment memory address by 1 byte
        addi t0, t0, -1       # Decrement counter by 1
        bne t0, x0, loop      

        li a7, 10            
        ecall