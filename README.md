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
make verify           # Lab1: assert reset, handoff, stack and SBI in GDB.
```

The root Makefile dispatches to `code/`; it does not duplicate the course build.
Python targets use `uv run --locked python`. Generated build products, virtual
environments, caches and personal IDE state are not committed.

See [docs/development.md](docs/development.md) for tooling and debugging details
and [docs/course/SUMMARY.md](docs/course/SUMMARY.md) for the course book.

## Completed Lab1 delivery

On branch `lab1`, see [the completed report](report/report.md),
[actual prompts](report/prompt.md), [preserved evidence](report/evidence/README.md),
and the real terminal screenshots in `report/images/`. Personal metadata stays
as explicit placeholders at the user's request.

`make verify` adds actual guest-state assertions to the original boot smoke
check; it is not the missing course grader. Current Lab1 has an empty BSS, so
the separate synthetic memset probe is identified explicitly. Research and
reproducibility notes are in [source notes](docs/lab1-source-notes.md) and
[verification notes](docs/lab1-verification.md).

Optional screenshot reproduction requires Xvfb, xterm, xwininfo and ImageMagick:
`uv run --locked python scripts/capture_lab1.py`. It captures real terminals
displaying saved output, not invented or live-debugger screenshots. These host
tools are not needed to build, boot or run `make verify`.
