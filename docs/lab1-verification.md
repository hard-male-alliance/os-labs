# Lab1 reproducible verification

## Contract and reproduction

Run from `/home/moesegfault/os-labs` in Ubuntu 24.04 WSL:

```sh
uv run --locked python scripts/verify_lab1.py
uv run --locked python .temp/test_verify_lab1.py
uv run --locked python .temp/test_verify_lab1_timeout.py
# The negative test overwrites normal evidence; restore the final successful run.
uv run --locked python scripts/verify_lab1.py
```

The verifier builds the authoritative `code/Makefile` `TARGETS`. It obtains CC,
CFLAGS, KCFLAGS, QEMU and GDB through `print-*`, and extracts QEMU arguments from
`make -n V= GDB_PORT=<ephemeral-port> debug`. It uses no duplicated QEMU boot
recipe or guessed toolchain. The compiler preprocesses `mmu.h` and `memlayout.h`
using the selected flags to determine KSTACKSIZE. The GDB process is launched
with `-nx --batch` so user debugger initialization cannot silently alter checks.

All subprocess operations have timeouts. The private QEMU listens only on
127.0.0.1, begins paused (`-S`), and is terminated/waited by its owning process
in `finally`. An unused-port selection has a small unavoidable bind race; a
QEMU early exit is a reported failure, never a connection to an old emulator.
No sudo, package installation, global pkill, or grader invocation occurs.
The default 45-second timeout can be overridden with `--timeout`.

## Retained evidence

Under `.cache/lab1/`:

| File | Contents |
| --- | --- |
| `verify-result.json` | Machine-readable assertions, reset opcodes, register observations, result. |
| `verify-gdb.log` | Raw debugger commands/results, disassembly, register dump and assertions. |
| `verify-qemu.log` | Raw OpenSBI and kernel serial output. |
| `verify-commands.gdb` | Generated batch script for the exact invocation. |
| `verify-tools.json` | Resolved Make tools/flags, actual QEMU and GDB arguments, stack expression. |
| `verify-build.log` | Build stdout for this invocation. |
| `verify-timeout-result.json` | Preserved negative timeout result, not a successful verification. |
| `verify-timeout-test.json` | Negative-test exit code and before/after QEMU PID sets. |

Normal evidence is overwritten on each invocation. These are local generated
artifacts, not invented screenshots or an official course grade.

## Actual observations (2026-10-01)

Tested with QEMU 8.2.2/default OpenSBI 1.3, `gdb-multiarch` 15.1 and Ubuntu's
`riscv64-linux-gnu` GCC 13 toolchain. The final verifier run passed 32 assertions.
Eight focused stdlib unit tests passed. The real-QEMU negative integration test
used a deliberately stalled Python debugger, returned exit code 1 with `GDB
timed out`, and found no new QEMU PIDs after cleanup (`before=[]`, `after=[]`).

Reset PC is `0x1000` in M mode. The actual six RV64 reset-ROM instructions were:

| Address | Decoded instruction | Effect |
| --- | --- | --- |
| `0x1000` | `auipc t0,0` | Set t0 to ROM base `0x1000`. |
| `0x1004` | `addi a2,t0,40` | Pass firmware dynamic-info address `0x1028`. |
| `0x1008` | `csrr a0,mhartid` | Read hart ID, observed 0. |
| `0x100c` | `ld a1,32(t0)` | Load DTB address, observed `0x87e00000`. |
| `0x1010` | `ld t0,24(t0)` | Load firmware target `0x80000000`. |
| `0x1014` | `jr t0` | Transfer to OpenSBI at `0x80000000`. |

The complete 12,400-byte raw kernel image already matched guest memory at
`0x80200000` while PC was still `0x1000`. Thus the tested QEMU `-kernel` path
preloads the image; watching the entry word for an OpenSBI write is not a
reliable loading experiment here. This is a measured distinction from the
course tip, not a claim about every platform's boot implementation.

At `kern_entry=0x80200000`, privilege is S and satp is 0. The log includes a0,
a1, a2, inherited firmware sp and ra. Actual entry instructions are:

```asm
0x80200000: auipc sp,0x3
0x80200004: ld sp,88(sp)       # load bootstacktop through GOT
0x80200008: j 0x8020000a      # compressed relaxed tail
```

After these three instructions, PC is `kern_init=0x8020000a`, sp is
`bootstacktop=0x80203000`, and ra is unchanged. The stack begins at
`bootstack=0x80201000`: size 8192 bytes and 16-byte top alignment. The harness
checks actual instruction boundaries and register effects, not an assumed
`auipc/addi` pseudo-instruction expansion. Symbol/address breakpoints avoid
fixed source lines and work with the optimized `-O2` build.

### Empty real BSS and separate synthetic experiment

The linked Lab1 has `edata=end=0x80203070`: **zero BSS bytes**. The original
kernel's `memset` is reached with a0=edata, a1=0, a2=0, then executed unchanged
through its return. No actual BSS bytes exist to poison or observe clearing;
the JSON explicitly records this, rather than counting vacuous poisoning or
clearing assertions as real write evidence.

Only after that original call returns, the harness performs a separately
labeled synthetic guest-memory probe: save 64 bytes starting at end and the
caller-visible registers; poison the bytes with 0xA5; redirect guest PC to
`memset` with count 64 and the original return address; verify all 64 bytes
become zero; restore the bytes and registers and assert the restored state.
This tests the actual linked clearing implementation but is **not evidence of
a nonempty real BSS**. No source file, ELF, disk image or linker layout is
modified. If a future build has nonempty BSS, the harness instead poisons the
real edata..end interval before the original kernel call and checks it afterward.

### Console and privilege boundary

The actual cprintf call is reached, then the decoded `ecall` instruction at
`0x8020048a` inside `sbi_console_putchar`. It has a7=1 (legacy console putchar),
a0=40 (the first character `(`), and S privilege. A temporary breakpoint at
`mtvec & ~3 = 0x80000428` observes the real OpenSBI trap entry with M privilege,
mcause=9 (environment call from S) and mepc=`0x8020048a`. Continuing to the
cprintf return reaches `0x8020003a` in S privilege. The captured serial output
contains `(THU.CST) os is loading ...`.

An earlier probe using `stepi` directly on ecall landed after the complete SBI
trap-return sequence, not at the first M-mode instruction. It observed mcause=9
but S privilege and therefore correctly failed the attempted M-entry assertion.
The production harness catches mtvec instead of assuming what debugger
single-step means across a trap. QEMU documents single-step IRQ/timer controls;
this observation is specifically from the tested QEMU/RISC-V configuration.

## Limits and interpretation

- This is boot-state verification and a console-path probe, not a full firmware
  instruction trace, formal ISA proof, full printf test or hardware validation.
- The six-instruction ROM path, fixed expected load address, legacy console SBI
  and QEMU CSR exposure are intentionally Lab1/QEMU-specific contracts. A
  changed platform should fail visibly rather than be silently accepted.
- The boot loop is intentional; kernel process termination is not required.
- `tools/grade.sh` is absent. No official course score is claimed.
- Focused test fixtures stay under root `.temp/`, with generated evidence under
  root `.cache/`, as required by workspace instructions.

## Primary references

- [QEMU GDB usage and single-step controls](https://www.qemu.org/docs/master/system/gdb.html)
- [GDB architecture Python API: actual instruction addresses, lengths and decoding](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Architectures-In-Python.html)
- [QEMU RISC-V firmware/kernel boot interface](https://www.qemu.org/docs/master/system/target-riscv.html)

## Screenshot platform finding

Real terminal capture initially failed because WSLg mounts `/tmp/.X11-unix`
read-only. An automatically assigned Xvfb display could not create its pathname
socket. The capture helper now disables TCP and the pathname `unix` transport,
using Linux abstract local sockets instead. This avoids modifying the user's
WSLg socket directory or opening an X server network port. The server receives
an automatic display number, and only this helper's Xvfb/XTerm children are
stopped. Panels are explicitly labeled recorded-output replay, and capture
rejects panels too tall to display completely. The screenshots were visually
inspected against the preserved logs.
