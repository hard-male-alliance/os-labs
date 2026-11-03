    .text
    .align 2
    .globl lab3_exception_probe
lab3_exception_probe:
    li a0, 0
    # Explicit encodings make both instruction lengths independent of GAS RVC choices.
    .2byte 0x9002                 # C.EBREAK
    addi a0, a0, 1
    .4byte 0x00100073             # EBREAK
    addi a0, a0, 1
    .2byte 0x0000                 # Permanently illegal compressed instruction
    addi a0, a0, 1
    .4byte 0x0000007b             # Unsupported 32-bit custom opcode
    addi a0, a0, 1
    ret
