#include <mmu.h>
#include <memlayout.h>

    .section .text,"ax",%progbits
    .globl kern_entry
# OpenSBI enters in S-mode with paging disabled. Replace its private stack
# before entering C; bootstacktop is the exclusive end of our downward stack.
kern_entry:
    la sp, bootstacktop

    # kern_init never returns. A tail jump leaves ra unchanged and creates no
    # return frame; the assembler/linker may relax it to a single jump.
    tail kern_init

.section .data
    # Keep the early stack outside the BSS range cleared by kern_init.
    .align PGSHIFT
    .global bootstack
bootstack:
    .space KSTACKSIZE
    .global bootstacktop
bootstacktop:
