#include <stdio.h>
#include <ulib.h>

int main(void)
{
    int value;
    // C division by zero is undefined; this case tests the ISA's DIVW.
    asm volatile("divw %0, %1, zero" : "=r"(value) : "r"(1));
    cprintf("value is %d.\n", value);
    panic("FAIL: T.T\n");
}
