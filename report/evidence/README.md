# Lab3 execution evidence

Collected on 2026-11-03 using the image built from `docker/Dockerfile`.
All guest output comes from actual QEMU execution. Report screenshots replay
these saved logs in real X terminals; they are not live debugger screenshots.

| File | Meaning |
| --- | --- |
| `build.log` | Normal kernel build |
| `boot.log` | Normal boot, ten `100 ticks` reports, SBI shutdown |
| `probe-build.log` | Isolated extra-guest build |
| `probe.log` | Four real 16/32-bit exceptions, resumed timer flow, shutdown |
| `verify.log` | Normal/probe verification summary |
| `validation.json` | Source hashes, tool versions, measured durations |
| `grade.log` | Course grading build/run output and 100/100 total |
| `grade-context.log` | Original course register/context test guest output |
| `grade-illegal.log` | Original illegal instruction and fallback guest output |
| `grade-timer.log` | Original sustained timer test guest output |
| `review-boot.log` | Independent reviewer normal-boot rerun |
| `review-probe.log` | Independent reviewer exception/timer rerun |
| `gdb.log` | Real timer-trap frame, SP, argument and cause assertions |

Reproduction commands are in `docs/lab3-development.md`. The course test cases
are unmodified; their QEMU boot option was corrected from the loader device to
`-kernel`. The original grader uses a timeout to stop each case; its timer test
loops after reporting at tick 200. By contrast, both `verify_lab3.py` boots must
terminate successfully via SBI shutdown, not a timeout.

The course grader was run before a source-only readability cleanup to the
imported Lab2 code (derived-address local constants and author markers). The
final normal/probe boots validate the resulting sources recorded in
`validation.json`; the trap implementation and course tests did not change.

Independent course-compliance conclusions and resolved documentation findings
are recorded in [the review](../../docs/lab3-course-review.md).
