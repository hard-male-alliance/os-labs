# Lab3: trap handling and executable delivery

## Scope and source continuity

The local course exercise removes `lab3_switch_test()` and the user/kernel
switching Challenge. Lab3 implements supervisor-stack trap entry/return, timer
interrupts, illegal-instruction recovery and breakpoint recovery.

The branch skeleton omitted the Lab2 implementations needed to compile it.
`page_init` and the Best-Fit implementation were carried forward from
`origin/lab2` at `8f7daa9`. Lab3's interrupt-protected allocation wrappers and
course test bodies remain intact. No Buddy/SLUB extension was imported.
Physical limits are derived from the existing KERNBASE/KERNTOP mapping rather
than introducing another independent address range.

## State ownership and invariants

- The interrupted supervisor stack owns a 36-word (288-byte) trap frame.
  Slots 0..31 correspond to integer registers. Slot 2 is the pre-trap SP;
  slots 32..35 hold sstatus, sepc, stval and scause.
- SAVE_ALL is unchanged. Its legacy `sbadaddr` spelling is locally mapped to
  `stval`, the current assembler spelling of the same CSR (0x143).
- The frame address is passed in a0. C can modify its return PC; restoration
  reloads sstatus/sepc and all writable integer registers, with SP last.
  Saved diagnostic state need not be written back to stval/scause.
- This is a single-hart kernel. `ticks` has one writer, the supervisor timer
  handler. The next event is armed before counting/reporting the current event.
- TICK_NUM remains 100. The exercise mentions SHUTDOWN_AFTER but the skeleton
  omits it; this implementation defines ten reports, i.e. shutdown at tick 1000.
- Only the two explicitly recoverable exception categories advance EPC.
  The low instruction halfword distinguishes RV64GC 16-bit/32-bit encodings.
  Thus C.EBREAK and EBREAK follow the same path rather than cause-specific
  fixed increments. Other exception categories retain frame diagnostics.

## Boot integration findings

1. The skeleton's loader device copied bytes but left OpenSBI's next address
   at zero on QEMU 10.0.13. The observed run never reached the kernel.
   QEMU's supported `-kernel` path supplies the entry/handoff information.
   Normal boot and the course grader now use that same boot interface.
2. After `satp` enables Sv39, instruction fetching still uses the physical
   boot PC until an explicit jump. The skeleton only mapped the higher half,
   so a `-kernel` run still stalled before C. Carrying forward Lab2's temporary
   identity leaf, high-address jump and removal of the identity leaf fixes it.
   `lla` keeps the early boot variable accesses explicitly PC-relative.
3. Contemporary GAS rejects the historical CSR name `sbadaddr`; the local
   spelling alias fixes this without changing SAVE_ALL or its frame layout.

The grader's patterns, points and guest tests are unchanged. The sole change
in `code/tools/grade.sh` is the boot option. `make grade` operates on a copy
under `.temp/lab3-grade`, leaves the working kernel untouched, and preserves
per-case guest output for diagnosis instead of deleting it.

## Container workflow

From the repository root:

```sh
docker build -t os-labs:lab3 -f docker/Dockerfile docker
docker run --init --rm --user "$(id -u):$(id -g)" \
  -v "$PWD:/workspace" os-labs:lab3 make
docker run --init --rm --user "$(id -u):$(id -g)" \
  -v "$PWD:/workspace" os-labs:lab3 make grade
docker run --init --rm --user "$(id -u):$(id -g)" \
  -v "$PWD:/workspace" os-labs:lab3 \
  python3 scripts/verify_lab3.py --output report/evidence
```

For an interactive boot, substitute `make qemu` and add `-it` to `docker run`.
On the task host, Docker group membership was present in the account database
but absent from the existing process groups; `sg docker -c 'docker ...'` made
it effective without changing permissions or touching unrelated containers.
The container only needs a normal bind mount, not privileged mode.

With native tools installed, use `make`, `make grade`, and `make verify`.
The last command uses the locked uv environment. Inside the image, either
invoke `python3` directly as above or use `make verify PYTHON=python3`.

For GDB, run `make debug` in one shell and
`gdb-multiarch -q -batch -x scripts/lab3-timer.gdb` in another shell in the same
container. The default guest endpoint is 127.0.0.1:1234. Override GDB_PORT for
manual debugging if needed; the supplied batch probe uses the default port.

## Verification and evidence

Actual run results and tool versions are in `report/evidence/`:

- Course grader: 35/35 context, 25/25 illegal instruction/fallbacks, 40/40 timer;
  total 100/100. No cancelled Challenge is injected.
- Normal boot: allocator self-check, ten reports, successful QEMU exit through
  the guest's SBI shutdown rather than host timeout.
- Extra guest: explicit encodings 0x9002, 0x00100073, 0x0000 and 0x0000007b
  return in sequence and preserve a0. SIE remains disabled during the early
  exception probe; sscratch is cleared. After enabling interrupts, execution
  resumes beyond tick 200 with SIE restored, then reaches ordinary shutdown.
- GDB at a real timer trap: frame size and saved-SP delta both 288, a0 equals
  the frame pointer, cause is 0x8000000000000005, sscratch is zero.

The extra test kernel is built only in `.temp/lab3-probe`; test hooks never
enter the submitted startup path. Source hashes and measured boot durations
are recorded by `scripts/verify_lab3.py`. It does not replace the course grader.
Screenshots are real X terminals replaying the retained output; the capture
script is `scripts/capture_lab3.sh` and does not synthesize guest execution.

## References and design implications

- [RISC-V supervisor ISA](https://docs.riscv.org/reference/isa/v20260120/priv/supervisor.html):
  sstatus/sepc determine return state; hardware does not save the integer
  context. Restore software-owned state before executing sret.
- [RISC-V compressed instructions](https://docs.riscv.org/reference/isa/v20260120/unpriv/c-st-ext.html):
  compressed and regular encodings coexist. Decode instruction width instead
  of assuming a fixed increment for an exception cause.
- [QEMU virt documentation](https://www.qemu.org/docs/master/system/riscv/virt.html):
  use the kernel-loading interface and the generated DTB for firmware handoff.
- [MIT xv6, traps and system calls](https://mit-pdos.github.io/xv6-riscv-book/trap.html):
  the hardware/assembly/C/return decomposition is the same teaching model.
  Its kernel/user entry separation is not imported into this supervisor-only
  exercise; the current SAVE_ALL explicitly uses the existing kernel stack.
