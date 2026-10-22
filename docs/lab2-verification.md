# Lab2 end-to-end verification

## Entry points and evidence

Run `make verify` (or `python3 scripts/verify_lab2.py code`) from the repository
root. The Python harness builds `code` using its authoritative Makefile before
starting guests; it does not clean the tree or modify teaching tests. A failed
build, missing completion marker, panic, timeout, or failed debugger assertion
returns a nonzero status.

All emulator and debugger processes are bounded and owned by the invocation.
The GDB endpoint binds to an ephemeral localhost TCP port, not the conventional
shared port 1234. Child cleanup never uses `pkill`, process-name matching, or
other users' emulator instances. The small port-selection/bind race is not
silenced: emulator startup or debugger attachment fails visibly if the chosen
port becomes unavailable.

Generated evidence remains in `.cache/lab2/`:

- `verify-build.log`: exact build output, including compiler diagnostics.
- `verify-ram-{32,64,128,256}.log`: unmodified QEMU serial/stderr streams.
- `verify-gdb.log`: unmodified GDB transcript.
- `verify-gdb-qemu.log`: serial output from the debugger-owned guest.
- `verify-commands.gdb`: generated debugger program and selected endpoint.
- `verify-gdb-result.json`: individual debugger assertions and observed values.
- `verify-result.json`: overall result, RAM matrix, raw-image SHA-256, and
  debugger assertions. It is reset at invocation start, so an aborted new run
  cannot be mistaken for an earlier successful run.
- `panels/*.txt`: clearly labeled excerpts of those recorded outputs for
  terminal screenshot capture. These are created only after a successful run.

The JSON and log files are the primary evidence. Their presence alone is not
success: inspect `passed` and the process exit status. Optional screenshots
are secondary, readable presentations of the underlying recorded evidence.

## Contracts exercised

Each RAM-matrix guest uses the production boot path: QEMU `virt`, default
OpenSBI firmware, and the actual raw `code/bin/ucore.img` via `-kernel`.
The four RAM configurations are 32, 64, 128, and 256 MiB. Serial evidence must
show the expected managed physical range. In particular, the 256 MiB machine
must still stop management at `PHYSICAL_MEMORY_END` (0x88000000), rather than
following device-tree RAM beyond the direct-map contract.

Every guest must report all of these completion markers, with no panic or
`FAIL` in its output:

1. The supplied physical-page allocator checks succeeded.
2. Best-Fit shadow-model traces passed.
3. Buddy rounding, shadow-model traces, and coalescing passed.
4. SLUB class boundaries, payload preservation, nonaliasing, and reclamation
   passed.
5. The complete extended allocator check suite passed.

A separate 128 MiB guest starts paused and is controlled by real RISC-V GDB.
Hardware address breakpoints stop at `kern_init` and
`allocator_checks_done`; software breakpoints cannot safely patch an unmapped
high virtual entry before the guest has enabled its page tables. Assertions
check:

- The C entry point is high virtual and executes with Sv39 translation.
- The initial stack lies within the linked boot stack and is ABI-aligned.
- After initialization and allocator checks, execution is supervisor mode,
  Sv39 remains active, and the physical-to-virtual offset matches the mapping.
- `npage` agrees with the managed physical limit.
- Best-Fit remains the selected default physical page manager.
- The Best-Fit free-page count matches the page-aligned initial free range.
- Walking the actual free list produces that same total; every block is
  positive, marked free, not reserved, address-ordered, and nonadjacent after
  coalescing. The walk is bounded by the physical page count.

This does not claim live CLion behavior or arbitrary hardware portability.
It verifies the repository's actual command-line user journey and debugger
integration on the course's QEMU platform.

## Real terminal screenshots

After successful verification, run `python3 scripts/capture_lab2.py`. It uses
optional host tools `Xvfb`, `xterm`, `xwininfo`, and ImageMagick `import`. Each
panel is printed by `cat` in a real XTerm window on a private virtual display;
the existing window is then captured to `report/images/lab2-*.png`. The title
explicitly states that it is recorded output, not a live debugging session.
No generated, painted, or reconstructed terminal images are used.

When promoting evidence into a course submission, retain the corresponding
logs/JSON (for example in `report/evidence/`) along with the screenshots.
Screenshots alone are not the complete verification artifact.

## Observed integration run (2026-10-22)

`make verify` completed successfully, followed by
`python3 scripts/capture_lab2.py`. All three screenshots were opened and
visually inspected for complete, readable content.

| RAM | Managed end | Initial/restored free pages | Result |
| --- | --- | --- | --- |
| 32 MiB | 0x82000000 | 7,589 | Passed all allocator completion markers |
| 64 MiB | 0x84000000 | 15,701 | Passed all allocator completion markers |
| 128 MiB | 0x88000000 | 31,925 | Passed all allocator completion markers |
| 256 MiB | 0x88000000 | 31,925 | Passed all allocator completion markers |

The separate debugger guest passed 17 assertions. Its free list contained
one fully coalesced block of 31,925 pages; the temporary identity mapping
at root index 2 was zero, root index 511 retained the expected
`(0x80000 << 10) | 0xcf` leaf, and `satp` referenced the expected physical
root page. The raw image SHA-256 was
`3b5e1a1e44a12cf3aa5b8756d1969dea908c17fee72df49a6f02330d023da10d`.
These values identify this integration run, not a promise that source or
layout changes retain identical page counts or image hashes.

Screenshots from the recorded evidence are
`report/images/lab2-allocator-checks.png`, `report/images/lab2-gdb.png`, and
`report/images/lab2-ram-matrix.png`.

The course grader was subsequently run independently and reported
`Total Score: 20/20` (page initialization and Best-Fit checks). Its complete
output was preserved as `report/evidence/grade.log` and captured in the real
terminal screenshot `report/images/lab2-grade.png`.

The complete submitted evidence snapshot is under `report/evidence/`,
including raw serial logs, debugger logs/results, grading output, panels,
and `manifest.json` with tool versions and SHA-256 hashes of sources and
recorded evidence. `original-entry-gdb.log` preserves the earlier boot-fault
diagnosis; it is negative evidence from the original assembly, not a failure
of the final image. Source changes require rerunning verification and
refreshing this evidence snapshot, rather than merely reusing the images.

The final rerun includes the strengthened Buddy aligned-hole allocation oracle
and unaligned/disjoint region registration checks. Host Best-Fit property
tests separately passed 50,000 model-checked operations; see `property.log`.
The same model run passed AddressSanitizer and UndefinedBehaviorSanitizer;
see `property-sanitized.log`. LeakSanitizer was disabled because the execution
sandbox does not support its ptrace requirement; this harness has no dynamic
host heap allocations. These host tests complement, not replace, guest runs.

An independent clean export also passed the four-size RAM matrix, 17 GDB
assertions, and 20/20 course grader with `riscv64-linux-gnu-` as the fallback
cross toolchain. Its separate results are
`report/evidence/clean-delivery.log` and
`report/evidence/clean-delivery-verify-result.json`. This confirmed the
pre-paging `lla` fix without relying on previously built ELF objects. The
Linux-GNU layout and image hash differ from the primary ELF-toolchain run;
compare each run to its own recorded baseline.
