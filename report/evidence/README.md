# Lab1 evidence provenance

Date: 2026-10-01 (UTC+8). These files are preserved results of real commands
in `/home/moesegfault/os-labs`, Ubuntu 24.04.5 under WSL2. They are not an
instructor grader, a formal proof, or a transcript of manually typed commands.

## What is preserved

- `final-suite.log`: commands, unmodified stdout/stderr, and exit codes for the
  final clean build, tool checks, lock check, boot smoke, eight unit tests,
  real QEMU timeout negative test, full GDB verification and binary comparison.
- `verify-gdb.log`, `verify-qemu.log`: complete raw output from the successful
  batch debugger invocation and its own emulator.
- `verify-result.json`: all 32 successful assertions and the six reset steps.
- `verify-tools.json`, `verify-commands.gdb`: exact resolved commands/flags and
  generated GDB program. The saved port is historical: do not launch this GDB
  file against an unrelated emulator. Re-run `make verify` for a fresh port.
- `verify-build.log`: build output inside the verifier; the preceding clean
  build is in `final-suite.log`.
- `smoke-qemu.log`: separate normal boot output without synthetic memory probes.
- `verify-timeout-test.json`, `verify-timeout-result.json`: deliberately failed
  stalled-debugger test; expected exit 1 and no leaked QEMU, not a normal pass.
- `environment.log`: real tool versions.
- `clean-checkout.log`: clean Git export creates its own environment, builds,
  boots, passes 32 assertions, matches the image and reproduces all screenshots.
- `panels/*.txt`: verbatim excerpts displayed for the screenshots. Source labels
  and separators were added, but no result values or assertion text were edited.
- `manifest.json`: capture time, baseline revision, image digest and SHA-256 of
  the preserved evidence and display panels. Runtime PID/port values naturally
  differ on another run. Screenshot hashes are also included after capture.

## Screenshots

`../images/01-build.png` through `05-boot.png` are real XTerm window captures.
A private Xvfb server renders the terminal while it displays recorded results.
This is expressly **output replay**, not a claim of live human GDB interaction.
No image generation or manual drawing of test output was used.

To reproduce screenshots from the committed panels in a fresh clone:

```sh
uv sync --locked
uv run --locked python scripts/capture_lab1.py
```

Optional host tools: Xvfb, xterm, xwininfo, and ImageMagick `import`. The capture
script does not install packages and touches only its own X server and XTerm.
If `.cache/lab1/panels/` exists, that local set takes precedence. For new actual
results run `make verify` and select unmodified excerpts of the new logs rather
than claiming the historical panels represent a new run.

## Re-run the substantive verification

```sh
uv sync --locked
make clean
make -j2
make doctor
make compdb
make smoke
make verify
```

The eight focused unit fixtures and timeout integration fixture remain local
under the repository `.temp/`, per workspace policy. Their actual outputs are
preserved here; the portable substantive boot verifier itself is committed.
Independent failure-injection checks are documented in
`../../docs/lab1-delivery-validation.md`. No unit-fixture existence is claimed
for a fresh Git clone.

## Interpretation boundaries

The real linked BSS is empty (`edata=end`). The original zero-count call runs
unchanged. A later separate 64-byte synthetic memset probe changes only guest
RAM/registers, verifies clearing and restores them before continuing. It is
not evidence that the normal boot cleared a nonempty BSS.

The image at reset already equals all 12,400 bytes of the disk image. QEMU's
reset ROM and image preloading must not be conflated with OpenSBI loading.
The initial raw image and final raw image have identical SHA-256:
`404d805a3f95bc4a2432e0db04055635ec9d014f51a0fc3a324f1986704fb17a`.
This does not assert identical ELF debug information or universal hardware
correctness. `code/tools/grade.sh` is absent; no course grade was fabricated.

Git treats this evidence directory as byte-preserved (-text), so default text
normalization cannot silently invalidate raw serial-output hashes. A clean
Git export independently confirmed all original evidence/image hashes.
