# Reproducing the Lab 5 guest and host GDB trace

Evidence: [RV64 guest](guest-gdb.log) and [x86-64 QEMU host](host-gdb.log).
The run used QEMU 10.0.13 (Debian `1:10.0.13+ds-0+deb13u1`) and GDB 16.3.

1. Build the default `hello` kernel with `make`. From `code/`, start QEMU with
   `qemu-system-riscv64 -machine virt -nographic -bios default -kernel bin/ucore.img -S -gdb tcp:127.0.0.1:12455`.
   Keep the host PID for the second debugger.
2. In the guest debugger, open `bin/kernel`, run
   `add-symbol-file obj/__user_hello.out`, and connect with
   `target remote localhost:12455`. Find the `ecall` in `sys_putc` and the
   `sret` in `__trapret` with the cross-toolchain's `objdump -d`. Hardware-break
   on the first address and continue. This run found `0x8000a2` and
   `0xffffffffc0200e36`; the addresses are build dependent.
3. From another terminal attach host GDB to the QEMU PID after the guest has
   stopped at its `ecall`. Break on `helper_raise_exception`,
   `riscv_cpu_do_interrupt`, and `helper_sret`. For each host stop, collect a
   backtrace, disable the breakpoint, and continue. The exported symbols are
   enough for backtraces; source lines and CPU state fields need debugging
   information matched by ELF build ID. Debian's `debuginfod.debian.net`
   provided the main build ID `f6156630694661b2ee9484661cfc204bdbe07950`
   and supplementary DWZ build ID `ab5d0dd87e126f2cb80a0c1e2f33cef44db5de1c`.
   Both were placed in the debugger's `.build-id` search directory. In the
   initial unprivileged container the host debugger and QEMU needed the same
   PID namespace, with ptrace allowed for the host debugger.
4. Disable the original guest breakpoint. Continue to a temporary hardware
   breakpoint on `__alltraps` conditioned on `$scause == 8`, then to the
   disassembled `sret` conditioned on `$scause == 8 && $sepc == 0x8000a6`.
   Finally, continue to a temporary breakpoint at `0x8000a6`. Record
   `pc`, `sepc`, `scause`, `sstatus`, and `a0` where applicable. The test
   stops on matching addresses instead of expecting the remote debugger to
   single-step every instruction between the guest and the trap handler.

On the observed x86-64 host, the helper's first two arguments are in `rdi`
and `esi` at function entry. With DWARF available, the corresponding guest PC
and privilege are `((CPURISCVState *)$rdi)->pc` and
`((CPURISCVState *)$rdi)->priv` at the exception helper, and the return PC
is `((CPURISCVState *)$rdi)->sepc` at the SRET helper. These are host ABI
registers, not the guest's `a0` and `a1`.
