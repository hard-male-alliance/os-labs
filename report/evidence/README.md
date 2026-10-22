# Lab2 recorded evidence

This directory is a checked-in snapshot of real local execution, not a set of
expected outputs. The reproduction commands are `make verify`, `make grade`,
and `make property`. Complete procedures and assertion contracts are in
[the verification notes](../../docs/lab2-verification.md).

| Artifact | Meaning |
| --- | --- |
| `verify-result.json` | Overall result, RAM matrix, actual image SHA-256, and 17 GDB assertions |
| `verify-ram-{32,64,128,256}.log` | Unmodified serial/stderr streams from actual QEMU guests |
| `verify-build.log` | Build output recorded by the verification harness |
| `verify-gdb.log` | Actual batch GDB transcript |
| `verify-gdb-qemu.log` | Serial output of the debugger-owned guest |
| `verify-gdb-result.json` | Individual debugger assertion values and outcomes |
| `grade.log` | Complete independent course-grader run, including 20/20 result |
| `property.log` | Host Best-Fit model checks, 50,000 operations |
| `property-sanitized.log` | Same host model run under AddressSanitizer and UndefinedBehaviorSanitizer |
| `original-entry-gdb.log` | Negative evidence diagnosing the original, subsequently fixed boot assembly |
| `manifest.json` | Tool versions, source hashes, recorded-evidence hashes, and exact image hash |
| `panels/*.txt` | Labeled recorded-output panels displayed for real terminal screenshots |

The sanitizer run disables LeakSanitizer because its ptrace mechanism is not
available in the execution sandbox; the property harness itself does not
allocate dynamic host heap objects. Address and undefined-behavior checking
remain enabled.

The kernel intentionally loops after its checks. QEMU termination messages at
log ends are the harness cleaning up its own guest, not kernel crashes. The
original-entry diagnosis is deliberately retained as historical failure
evidence; final success is determined by `verify-result.json` and `grade.log`.

Screenshots in [../images/](../images/) are captures of actual XTerm windows
showing these recorded panels, not generated terminal graphics. Their titles
explicitly identify recorded output. `scripts/capture_lab2.py` runs private
Xvfb/XTerm processes and captures them with ImageMagick; it never captures a
user's existing display. The screenshots supplement, rather than replace,
the raw logs and machine-readable results.

Fresh runs write `.cache/lab2/`; they do not silently replace this submitted
snapshot. After changing code, rerun verification and grading, then refresh
the copied evidence, panels, screenshots, and manifest together. Do not
interpret old screenshots as evidence for changed sources. The source tree
contains intentionally unfilled student metadata; the manifest records the
implementation content independently of a final Git commit.

## Independent clean export / fallback compiler

`clean-delivery.log` records a separate export with no ignored local build
outputs, compiled using the Linux-GNU cross compiler rather than the default
ELF cross compiler. Its RAM matrix and 17 debugger assertions are preserved
as `clean-delivery-verify-result.json`; the same delivery run also passed the
course grader. Compiler-dependent code layout, image hashes, and free-page
counts need not equal the main ELF baseline. `linux-pic-first-boot.log` is
historical negative evidence showing why pre-paging assembly must explicitly
use PC-relative `lla`, rather than a `la` pseudo-instruction that may load a
high virtual pointer through the GOT under position-independent defaults.
