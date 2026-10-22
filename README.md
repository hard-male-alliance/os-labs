# OS Labs 2026

This repository separates shared tooling from individual course submissions.
The course delivery requirements are in [docs/course-delivery.md](docs/course-delivery.md).

## Branch layout

- `main`: shared uv configuration, locked Python dependencies, formatting rules,
  Git conventions, course documentation and development helpers. No lab source
  or report is submitted on this branch.
- `milestone/common-base`: annotated Git tag marking the shared starting point.
- `lab1`, `lab2`, ...: one branch per experiment, each created from the milestone,
  not from the preceding lab branch.

Each experiment branch adds:

```text
code/                 # Course source, Makefile and linker script.
report/
  report.md           # Experiment report following the shared template.
  prompt.md           # Actual prompts used for this experiment.
  images/             # Real test screenshots referenced by the report.
```

Reports may start as explicitly marked drafts. Do not claim exercise completion,
passing grader results, or screenshot evidence that has not been produced.

## Start another experiment

```sh
git fetch origin --tags
git switch -c lab2 milestone/common-base
# Add that experiment's source under code/ and its report under report/.
git add code report
git commit -m "feat(lab2): add experiment source and report"
git push -u origin lab2
```

Do not move the milestone tag after it has been published. Later shared changes
belong on `main`; apply them to active experiment branches deliberately. If a
new common baseline is needed, publish a new milestone tag rather than rewriting
the existing tag or silently changing old experiment bases.

## Development

`main` intentionally has no `code/`, so switch to an experiment branch before
building. On an experiment branch:

```sh
uv sync --locked
make
make doctor
make compdb
make qemu
```

The root Makefile dispatches to `code/`; it does not duplicate the course build.
Python targets use `uv run --locked python`. Generated build products, virtual
environments, caches and personal IDE state are not committed.

See [docs/development.md](docs/development.md) for tooling and debugging details
and [docs/course/SUMMARY.md](docs/course/SUMMARY.md) for the course book.

## Lab2: physical memory management

This branch implements the supplied Lab2 exercises: physical page initialization,
Best-Fit, Buddy System, and a simplified SLUB object allocator. Best-Fit remains
selected for the course-facing PMM. Buddy is integrated and verified through an
isolated guest arena; SLUB uses the active PMM without changing its interface.

```sh
make doctor           # Check the selected cross toolchain, QEMU and GDB.
make                  # Build the RV64 kernel and raw image.
make compdb           # Generate 16 compilation commands for IDE indexing.
make qemu             # Boot and execute the supplied and extended self-checks.
make debug            # Start paused; attach using make gdb in another terminal.
make grade            # Run the unchanged course tests in a temporary tree.
make verify           # Four RAM configurations and real guest GDB assertions.
make property         # 50,000 host-model operations on production Best-Fit code.
```

`make grade` checks page initialization and Best-Fit (20/20 in the recorded run),
not every exercise. Extended guest checks cover Buddy and SLUB, ownership,
payload preservation, OOM and complete page reclamation. `make verify` needs
permission to open a private localhost GDB port. The kernel intentionally loops
after the checks; `Ctrl-a x` exits interactive QEMU.

CLion configurations are named `Lab2 Build`, `Lab2 QEMU`,
`Lab2 QEMU GDB Server`, and `Lab2 Remote Debug`. Reload the Makefile project
after switching branches. Run the GDB server before attaching Remote Debug.
A live CLion re-import is not implied by CLI validation.

Start with [the report](report/report.md), [actual prompts](report/prompt.md),
[preserved evidence](report/evidence/README.md), and
[verification/reproduction notes](docs/lab2-verification.md). The historical
[import baseline](docs/lab2-integration.md) records the original teaching archive;
implementation rationale is in the [memory](docs/lab2-memory-notes.md),
[Buddy](docs/lab2-buddy-notes.md), [SLUB](docs/lab2-slub-notes.md),
[test plan](docs/lab2-test-plan.md), and [independent review](docs/lab2-review.md)
notes. Actual student identities and `YOUR CODE` markers remain explicit
placeholders until the user supplies the course metadata.

The supported platform is single-hart QEMU `virt`, with the supplied trusted
DTB parser and a 128 MiB managed-memory ceiling. Larger emulated RAM is detected
but intentionally capped. The simplified SLUB is not Linux's concurrent allocator.

Optional screenshot reproduction requires Xvfb, xterm, xwininfo and ImageMagick:
`uv run --locked python scripts/capture_lab2.py`. It captures real terminal
windows displaying saved evidence panels, not invented or live-session images.
