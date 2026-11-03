set pagination off
set confirm off
set architecture riscv:rv64
file code/bin/kernel
target remote 127.0.0.1:1234
break trap
continue
printf "trapframe bytes=%u\n", sizeof(struct trapframe)
printf "trapframe=%#lx saved-sp=%#lx delta=%lu\n", tf, tf->gpr.sp, tf->gpr.sp - (unsigned long)tf
printf "saved cause=%#lx epc=%#lx status=%#lx\n", tf->cause, tf->epc, tf->status
printf "handler a0=%#lx sscratch=%#lx\n", $a0, $sscratch
if sizeof(struct trapframe) != 288
  quit 1
end
if tf->gpr.sp - (unsigned long)tf != 288
  quit 1
end
if $a0 != (unsigned long)tf
  quit 1
end
if tf->cause != 0x8000000000000005
  quit 1
end
if $sscratch != 0
  quit 1
end
printf "PASS: real timer trap layout, original SP and a0 frame argument\n"
detach
quit
