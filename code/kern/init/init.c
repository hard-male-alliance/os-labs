#include <stdio.h>
#include <string.h>
#include <sbi.h>

/**
 * Initialize the freestanding kernel after entry.S establishes the boot stack.
 *
 * Clear [edata, end) before using zero-initialized globals, emit the original
 * course banner through SBI, and remain in the kernel without returning to
 * firmware. No scheduler or interrupt-driven idle loop exists in lab1.
 */
int kern_init(void) __attribute__((noreturn));

/** Complete minimal kernel startup; this function intentionally never returns. */
int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    const char *message = "(THU.CST) os is loading ...\n";
    cprintf("%s\n\n", message);
    while (1)
        ;
}
