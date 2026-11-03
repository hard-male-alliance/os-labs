# Lab3 prompt and specification record

## Actual user request

> 哦哦，好的。接下来你去实现 lab3 吧。需要的时候自己开 docker 用。

The following specifications were developed by the assistant from the checked-in
course instructions and source before implementation. They are implementation
working prompts, not additional messages attributed to the user. The teacher's
removal of the user/kernel switching Challenge is recorded in the course exercise.

## Module 1: trap entry and restoration

[PROMPT]
Implement RESTORE_ALL and __alltraps in code/kern/trap/trapentry.S. Preserve
SAVE_ALL, struct trapframe, public signatures and the original lab3 markers.

[RELY]
SAVE_ALL allocates 36 REGBYTES on the current kernel stack. Slots 0..31 are
x0..x31; slot 2 contains the pre-trap sp read through sscratch. Slots 32..35
contain sstatus, sepc, stval and scause. C takes its first argument in a0.
This experiment handles traps on the existing supervisor stack.

[GUARANTEE]
RESTORE_ALL; __alltraps; __trapret followed by sret.

[SPECIFICATION]
__alltraps:
Pre-Condition: hardware has entered the supervisor trap vector on the existing
16-byte-aligned kernel stack, with trap CSRs describing the interrupted state.
Post-Condition: SAVE_ALL builds the frame, C receives exactly its address as
the first argument, and the C return reaches __trapret with sp still at the frame.

RESTORE_ALL:
Pre-Condition: sp addresses the complete 36-word frame produced by SAVE_ALL,
including any return-state changes made by the C handler.
Post-Condition: the frame's epc/status changes are honored; all writable integer
registers and the original sp are restored before the following sret.

Requirements:
Restore sp last, after all frame accesses. Restore status/epc before scratch
registers. stval/scause are diagnostic values, not return control state.
Do not introduce the cancelled user/kernel switching Challenge.

## Module 2: interrupts and exceptions

[PROMPT]
Implement interrupt_handler and exception_handler in code/kern/trap/trap.c,
using the framework's timer, diagnostic and SBI shutdown interfaces.

[RELY]
struct trapframe, IRQ_* and CAUSE_* in riscv.h, ticks, clock_set_next_event,
print_ticks, print_trapframe, sbi_shutdown. RV64GC uses 16/32-bit instructions.
The skeleton defines TICK_NUM=100 but omits the documented SHUTDOWN_AFTER.

[GUARANTEE]
void interrupt_handler(struct trapframe *tf);
void exception_handler(struct trapframe *tf);
Add SHUTDOWN_AFTER=10 (ten reports, 1000 timer interrupts) and include sbi.h.
Keep existing trap.c functions and signatures unchanged.

[SPECIFICATION]
interrupt_handler:
Pre-Condition: tf points to a valid frame describing the interrupt. For a
supervisor timer interrupt, clock initialization has established the timer and
zeroed ticks; trap entry has disabled supervisor interrupts.
Post-Condition: the handler classifies the cause while preserving the frame's
return state. A supervisor timer event arms the next event and increments ticks
exactly once; each complete report interval produces the required output, and
the configured report count triggers SBI shutdown. Other known causes print
their category; unknown causes print the cause and complete frame.

exception_handler:
Pre-Condition: tf points to a valid exception frame. For illegal-instruction and
breakpoint exceptions, epc points to the readable 16/32-bit triggering instruction
in the current kernel code mapping.
Post-Condition: illegal-instruction and breakpoint exceptions print the required
diagnostics and advance epc by exactly the triggering instruction's width.
Other causes print the complete frame without changing epc. The remaining frame
fields are preserved for the common return path.

Requirements:
Timer: arm the next event, increment ticks once, print at each multiple of
TICK_NUM, and shut down after SHUTDOWN_AFTER reports. Recognized non-timer
interrupts print their category. Unknown causes print the cause and full frame.
Illegal instruction/breakpoint: print the exception and fault address; advance
epc by 2 or 4 according to instruction bits [1:0], so execution continues once.
Other exceptions print the frame without inventing recovery semantics.

## Integration and delivery

[PROMPT]
Reuse the implemented page_init and Best-Fit allocator from origin/lab2 rather
than redesigning Lab2. Keep Lab3's interrupt-protected allocator wrappers and
existing course tests. Build with a Docker RISC-V toolchain and boot real QEMU.
Check compressed and uncompressed exception returns, register restoration,
sustained timer delivery, reporting cadence and actual shutdown. Keep evidence
and an experiment report; do not add the cancelled Challenge.

[RELY]
origin/lab2 at 8f7daa9; checked-in course grader, injector and context tests;
RISC-V privileged/unprivileged ISA, QEMU virt boot documentation and xv6 traps.

[GUARANTEE]
Working normal boot and course grading path, focused supplemental guest checks,
reproducible container workflow, report/report.md and actual terminal evidence.

[SPECIFICATION]
Pre-Condition: the clean Lab3 skeleton and implemented Lab2 dependency sources
are available, along with the course tests and a working Docker daemon.
Post-Condition: the submitted kernel builds and completes normal boot through
timer-driven shutdown; isolated grading/probe kernels exercise the required
trap behaviors; the report, prompts, screenshots and retained logs describe
the observed results. No test injection enters the submitted startup path.

Requirements:
Retain public entry points and course test semantics. Fix tooling integration
where real execution exposes incompatibility. Do not hide failing output or
claim tests that were not run. Record observed problems and fixes after running.

## Independent-review refinement

The review made the entry/restore preconditions distinct and added explicit
per-handler and integration pre/postconditions to match the course prompt
template. These clarify the existing behavior; no kernel code changed for
this documentation refinement.
