#include <defs.h>
#include <mmu.h>
#include <memlayout.h>
#include <clock.h>
#include <trap.h>
#include <riscv.h>
#include <stdio.h>
#include <assert.h>
#include <console.h>
#include <vmm.h>
#include <pmm.h>
#include <kdebug.h>
#include <unistd.h>
#include <syscall.h>
#include <error.h>
#include <sched.h>
#include <sync.h>
#include <sbi.h>

#define TICK_NUM 100

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S */
void idt_init(void)
{
    extern void __alltraps(void);
    /* Set sscratch register to 0, indicating to exception vector that we are
     * presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
    /* Allow kernel to access user memory */
    set_csr(sstatus, SSTATUS_SUM);
}

/* trap_in_kernel - test if trap happened in kernel */
bool trap_in_kernel(struct trapframe *tf)
{
    return (tf->status & SSTATUS_SPP) != 0;
}

void print_trapframe(struct trapframe *tf)
{
    cprintf("trapframe at %p\n", tf);
    // cprintf("trapframe at 0x%x\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  tval 0x%08x\n", tf->tval);
    cprintf("  cause    0x%08x\n", tf->cause);
}

void print_regs(struct pushregs *gpr)
{
    cprintf("  zero     0x%08x\n", gpr->zero);
    cprintf("  ra       0x%08x\n", gpr->ra);
    cprintf("  sp       0x%08x\n", gpr->sp);
    cprintf("  gp       0x%08x\n", gpr->gp);
    cprintf("  tp       0x%08x\n", gpr->tp);
    cprintf("  t0       0x%08x\n", gpr->t0);
    cprintf("  t1       0x%08x\n", gpr->t1);
    cprintf("  t2       0x%08x\n", gpr->t2);
    cprintf("  s0       0x%08x\n", gpr->s0);
    cprintf("  s1       0x%08x\n", gpr->s1);
    cprintf("  a0       0x%08x\n", gpr->a0);
    cprintf("  a1       0x%08x\n", gpr->a1);
    cprintf("  a2       0x%08x\n", gpr->a2);
    cprintf("  a3       0x%08x\n", gpr->a3);
    cprintf("  a4       0x%08x\n", gpr->a4);
    cprintf("  a5       0x%08x\n", gpr->a5);
    cprintf("  a6       0x%08x\n", gpr->a6);
    cprintf("  a7       0x%08x\n", gpr->a7);
    cprintf("  s2       0x%08x\n", gpr->s2);
    cprintf("  s3       0x%08x\n", gpr->s3);
    cprintf("  s4       0x%08x\n", gpr->s4);
    cprintf("  s5       0x%08x\n", gpr->s5);
    cprintf("  s6       0x%08x\n", gpr->s6);
    cprintf("  s7       0x%08x\n", gpr->s7);
    cprintf("  s8       0x%08x\n", gpr->s8);
    cprintf("  s9       0x%08x\n", gpr->s9);
    cprintf("  s10      0x%08x\n", gpr->s10);
    cprintf("  s11      0x%08x\n", gpr->s11);
    cprintf("  t3       0x%08x\n", gpr->t3);
    cprintf("  t4       0x%08x\n", gpr->t4);
    cprintf("  t5       0x%08x\n", gpr->t5);
    cprintf("  t6       0x%08x\n", gpr->t6);
}

//lab3 function1 2413077 2410824 2413174
static void interrupt_handler(struct trapframe *tf)
{
    uintptr_t irq = tf->cause & ~((uintptr_t)1 << (sizeof(uintptr_t) * 8 - 1));
    if (irq == IRQ_S_TIMER)
    {
        clock_set_next_event();
        if (++ticks % TICK_NUM == 0)
            cprintf("%d ticks\n", TICK_NUM);
        if (current != NULL)
            current->need_resched = true;
        return;
    }
    print_trapframe(tf);
    panic("unexpected interrupt");
}

//lab5 function1 2413077 2410824 2413174
void exception_handler(struct trapframe *tf)
{
    if (tf->cause == CAUSE_USER_ECALL)
    {
        tf->epc += 4;
        syscall();
        return;
    }
    if (!trap_in_kernel(tf))
    {
        int ret = -E_FAULT;
        if (tf->cause == CAUSE_STORE_PAGE_FAULT)
            ret = resolve_cow(current->mm->pgdir, tf->tval);
        if (ret == 0)
            return;
        cprintf("user fault: pid=%d, cause=%lu, epc=%p, address=%p\n",
                current->pid, tf->cause, (void *)tf->epc, (void *)tf->tval);
        do_exit(ret);
    }
    print_trapframe(tf);
    panic("unexpected supervisor exception");
}

static inline void trap_dispatch(struct trapframe *tf)
{
    if ((intptr_t)tf->cause < 0)
    {
        // interrupts
        interrupt_handler(tf);
    }
    else
    {
        // exceptions
        exception_handler(tf);
    }
}

/* Restore the previous frame for nested S-mode traps before scheduling. */
void trap(struct trapframe *tf)
{
    if (current == NULL)
    {
        trap_dispatch(tf);
        return;
    }

    struct trapframe *previous = current->tf;
    current->tf = tf;
    trap_dispatch(tf);
    current->tf = previous;

    if (trap_in_kernel(tf))
        return;
    if (current->need_resched)
        schedule();
    if (current->flags & PF_EXITING)
        do_exit(-E_KILLED);
}
