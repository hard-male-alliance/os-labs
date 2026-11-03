#include <assert.h>
#include <clock.h>
#include <riscv.h>
#include <stdio.h>

extern uintptr_t lab3_exception_probe(void);

void lab3_check_exceptions(void) {
    assert((read_csr(sstatus) & SSTATUS_SIE) == 0);
    assert(lab3_exception_probe() == 4);
    assert((read_csr(sstatus) & SSTATUS_SIE) == 0);
    assert(read_csr(sscratch) == 0);
    cprintf("[Extra] 16/32-bit breakpoints and illegal instructions: PASS\n");
}

void lab3_check_timer(void) {
    while (ticks < 200) {
        asm volatile("" ::: "memory");
    }
    assert(read_csr(sstatus) & SSTATUS_SIE);
    cprintf("[Extra] Repeated timer return with SIE restored, ticks=%lu: PASS\n", ticks);
}
