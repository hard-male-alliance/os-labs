# Lab2 integration baseline

## Input and scope

The `lab2` branch was created from `milestone/common-base` (`5e855c9`).
The user supplied `lab2.zip`, with SHA-256
`48402e9b2dba7cf4726060a0bda3fa0c57e4f7be66f76ecf11851e181f546d6c`.
Its 39 files were extracted from `lab2_new/` into `code/`, without the
extra enclosing directory. The original archive was removed after extraction at the user's request;
the checksum above records the input provenance.

This task integrates the teaching skeleton, not the exercise solutions.
All 37 files other than `Makefile` and `tools/grade.sh` are byte-identical
to their archived versions, including the kernel and supplied test bodies.
The report is an explicitly unfinished course-template draft, with no
claimed results or screenshot placeholders pointing at nonexistent images.
Existing personal IDE edits were preserved.

## Integration decisions

- Retain the course Makefile and root dispatch model. Add root `test` and
  `grade` forwarding; do not introduce a second build system.
- Prefer `riscv64-unknown-elf-`, falling back to `riscv64-linux-gnu-`, while
  allowing explicit `GCCPREFIX` overrides. Select a RISC-V GDB or
  `gdb-multiarch`; use the same `GDB_PORT` for CLI server and client.
- Use `-kernel bin/ucore.img` with default OpenSBI in QEMU run, debug, test,
  and grading commands, instead of the distributed generic loader device.
  This follows the earlier workspace boot compatibility finding and the
  [QEMU firmware boot interface](https://www.qemu.org/docs/master/system/target-riscv.html#risc-v-cpu-firmware).
  GDB endpoints bind to localhost. No kernel entry or page-table changes
  are included in this integration.
- Keep grading copies under repository-local `.temp/lab2-grade/`, not `/tmp`.
  Failed grading retains its copy for diagnosis. Propagate failed builds
  instead of launching QEMU against a missing image. Reap the grader's own
  emulator on exit; supplied test expectations remain unchanged.
- Root Make targets default the uv cache to `.cache/uv/` to work in the
  restricted workspace. Direct uv commands can use
  `UV_CACHE_DIR="$PWD/.cache/uv" uv ...` for the same behavior.
- Reuse the established IDE configuration format, renamed to `Lab2 Build`,
  `Lab2 QEMU`, `Lab2 QEMU GDB Server`, and `Lab2 Remote Debug`.
  Remote Debug uses `code/bin/kernel` and `127.0.0.1:1234`.

## Validation and remaining work

Validation was run from the repository root on 2026-10-22:

| Check | Result |
| --- | --- |
| `make clean` | Removed stale Lab1 objects and images remaining after the branch switch. |
| `make doctor` | Passed: ELF cross compiler/binutils, QEMU and `gdb-multiarch` available. |
| `make compdb` | Passed: 13 entries, exact coverage of all kernel C/assembly sources. |
| `make` | Expected failure: Best-Fit tests reference missing `free_list` and `nr_free` state. |
| `make grade` | Test injection into the temporary copy succeeded; compilation failed on unfinished Best-Fit and missing `page_init()`, and the failure propagated. |
| `make -C code -n debug gdb test` | Selected commands use the intended image, debugger and localhost endpoint. |
| `sh -n code/tools/grade.sh` | Passed shell syntax validation. |
| Archive comparison | Exactly the two integration files differ; kernel and test sources are unchanged. |
| IDE configuration parsing | All four new XML files parse successfully. |

Logs are in `.cache/lab2/`: `clean.log`, `tooling.log`, `build.log`,
`grade.log`, and `debug-recipes.log`. Local logs are not course submission
evidence. No Lab2 kernel image, boot success, grader score, guest breakpoint,
or successful live CLion re-import has been established.

Next implement `page_init()` in `kern/mm/pmm.c`, implement and register the
Best-Fit manager in `kern/mm/best_fit_pmm.c`, then select it in
`init_pmm_manager()`. Preserve the supplied tests. Requirements, including
the additional allocation tasks, are in
[the local exercise document](course/lab2/lab2_2_1_exercise.md).
After implementation, run `make`, `make smoke`, `make grade`, and a real
guest GDB breakpoint. Reload the CLion Makefile project and verify indexing
in the IDE; compilation database generation alone is not proof of live indexing.
