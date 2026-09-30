# Lab 1 requirements and source evidence

Date: 2026-10-01 (Asia/Singapore). Repository: /home/moesegfault/os-labs.
Initial inspected revision: 74d683ded626b3704d5c1abed7ec6f17c590fb57, branch lab1.
This source audit does not itself claim runtime tests passed.

## Acceptance checklist

Internal authority: docs/course-delivery.md, docs/course/lab1/lab1_2_1_exercise.md,
docs/course/lab1/lab1_5_requirement.md, and docs/course-template report template.
There are two explanatory/debugging exercises, not an assigned implementation
of traps, scheduling or paging.

| Item | Required deliverable/evidence |
| --- | --- |
| Build and boot | Clean build and actual QEMU loading string; commands and tool versions |
| Exercise 1 | Explain address-loading la, stack range/downward growth/alignment, tail jump without new ra, noreturn; actual linked instructions and observed sp |
| Exercise 2 | Actual reset PC 0x1000 and six reset instructions; step to firmware 0x80000000; breakpoint at first kernel instruction 0x80200000; registers and handoff |
| Analysis | Chapter sequence, core modules/functions, lab-to-OS-principle mapping including differences, important uncovered OS principles |
| Template | Environment, actual AI tooling, actual prompts/iteration history, real test images, summary |
| Package | lab1 branch; code/ and report/; report.md, prompt.md and images/ containing all cited screenshots |
| Remote submission | Course requires git upload; verify remote/push separately; local commit is not uploaded submission |
| Integrity | No invented member IDs/model versions/results; unknown personal metadata explicitly unavailable; tools/grade.sh absent, so no invented make grade pass |

Keep transient logs/experiments under repository .cache or .temp.
Screenshot evidence must capture actual results rather than fabricated output.

## QEMU boot correction

QEMU v8.2.2 virt_memmap defines MROM at 0x1000 and DRAM at 0x80000000.
virt_machine_done loads firmware at DRAM start, invokes riscv_load_kernel,
then creates reset ROM. This describes this lab's ordinary virt/-kernel
path, not pflash or KVM boot.
[Memory map](https://github.com/qemu/qemu/blob/v8.2.2/hw/riscv/virt.c#L79-L100);
[Machine completion](https://github.com/qemu/qemu/blob/v8.2.2/hw/riscv/virt.c#L1178-L1215).

The 0x1000 reset trampoline is QEMU-generated ROM, not OpenSBI itself.
boot.c riscv_setup_rom_reset_vec emits auipc t0,0; addi a2,t0,40;
csrr a0,mhartid; ld a1,32(t0); ld t0,24(t0); jr t0.
It supplies dynamic firmware information, hart ID and device tree pointer,
then jumps to firmware. riscv_load_kernel loads the raw image host-side;
RV64 kernel start is firmware-end rounded up to 2 MiB, here 0x80200000.
The kernel bytes already exist while PC is paused at reset. A guest
write watchpoint therefore does not prove a later OpenSBI loading event.
Inspect bytes before stepping, then use an execution breakpoint for handoff.
This corrects the course tips without changing their educational objective.
[Kernel alignment](https://github.com/qemu/qemu/blob/v8.2.2/hw/riscv/boot.c#L64-L70);
[Kernel loader](https://github.com/qemu/qemu/blob/v8.2.2/hw/riscv/boot.c#L198-L253);
[Reset vector](https://github.com/qemu/qemu/blob/v8.2.2/hw/riscv/boot.c#L349-L398).

OpenSBI v1.3 sbi_hart_switch_mode sets mstatus.MPP to next_mode, writes
mepc=next_addr, clears satp for S-mode, passes a0/a1, then executes mret.
The kernel executes in S-mode, not M-mode because it uses physical addresses.
Confirm installed firmware version from the actual banner; this versioned
source explains expected behavior but does not replace a runtime trace.
[OpenSBI handoff](https://github.com/riscv-software-src/opensbi/blob/v1.3/lib/sbi/sbi_hart.c#L720-L777).

RISC-V Privileged ISA v1.13 (20240411) specifies reset privilege M and
implementation-defined reset PC: 0x1000 is a QEMU platform fact, not a
universal RISC-V address. MPRV can make M-mode data accesses use MPP's
effective privilege; it does not affect instruction-fetch privilege.
[Machine ISA, Reset and Memory Privilege](https://docs.riscv.org/reference/isa/v20240411/priv/machine.html).
satp is active for effective S/U privilege. MODE=Bare makes supervisor
virtual and physical addresses equal while physical protection still applies.
satp=0 means no address translation, not entry to M-mode or initialized paging.
[Supervisor ISA, satp](https://docs.riscv.org/reference/isa/v20240411/priv/supervisor.html).

## Entry and ABI

Initial code/kern/init/entry.S reserves two 4096-byte pages in .data,
aligned by .align PGSHIFT. bootstacktop is one-past-end, not a memory value
to dereference. la establishes the kernel's own stack before calling C.
kern_init zeroes [edata,end), outputs the loading message, and loops.
The stack is in .data, outside BSS clearing. Inspect linked binary to
determine actual instruction count and any linker relaxation.

The assembly manual specifies non-PIC la as address computation through
auipc/addi; PIC changes expansion. tail normally expands to auipc plus
jalr x0 through a temporary and may be shortened by linker relaxation.
It establishes no new return address.
[Assembly manual, Load Address/Function Calls](https://github.com/riscv-non-isa/riscv-asm-manual/blob/main/src/asm-manual.adoc).
The standard ABI requires downward-growing stacks with 16-byte alignment
at entry and throughout execution. A page-aligned 8192-byte stack meets it.
[RISC-V psABI integer calling convention](https://riscv-non-isa.github.io/riscv-elf-psabi-doc/#_integer_calling_convention).

Output flow to verify in current source:
kern_init -> cprintf/vcprintf -> vprintfmt -> cputch -> cons_putc ->
sbi_console_putchar -> sbi_call -> ecall -> OpenSBI -> virtual UART.

## Bounded quality risks

1. Initial libs/sbi.c manually writes a0-a2/a7 inside inline assembly but
   declares only a generic output and memory clobber. GCC does not know
   these registers were overwritten and can allocate conflicting operands.
   Fixed-register input/output constraints are preferable. Legacy SBI
   preserves every register except a0, ignores a6 and returns a legacy-specific
   a0 value, unlike modern two-register results.
   [SBI v2.0 legacy specification](https://github.com/riscv-non-isa/riscv-sbi-doc/blob/v2.0/src/ext-legacy.adoc).
2. kern_entry initially resides in generic .text; changing object ordering
   could displace raw-image entry. Dedicated .text.kern_entry and linked
   entry/base checks are more robust; ELF ENTRY alone does not control the
   host-side raw-image load/jump.
3. Runtime evidence must use current ELF symbols for stack/BSS/C entry,
   not stale tutorial example addresses.
4. Preserve loading string, public names, linker base and Make targets.
   No scheduler, paging subsystem or driver replacement is needed.

## Supplementary peer-reviewed connection (not a lab requirement)

Klein et al., Comprehensive Formal Verification of an OS Microkernel,
ACM TOCS 32(1), Article 2 (2014), DOI 10.1145/2560537, connects specifications,
C and binary semantics. Its explicit trusted assumptions still include
hand-written assembly, boot code, caches and hardware (PDF pp. 3-4).
The transferable lesson is modest: state boot/ABI invariants and check
compiled/runtime behavior, rather than equating source intention with
execution. This lab is not formally verified; seL4 proves nothing about it.
[Author-hosted peer-reviewed paper](https://sel4.systems/Research/pdfs/comprehensive-formal-verification-os-microkernel.pdf).
