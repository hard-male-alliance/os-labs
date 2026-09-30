# Independent Lab1 delivery validation

Date: 2026-10-01 (Asia/Singapore). Repository: `/home/moesegfault/os-labs`.
This validation does not alter production code, rebuild concurrently, or overwrite
another agent's `.cache/lab1` logs. Test fixtures and private results remain in
`.temp/lab1-delivery-validation/`.

## Expected behavior and authority

The two exercises in `docs/course/lab1/lab1_2_1_exercise.md` require explanations
of stack-address initialization and tail transfer, plus GDB observation from
reset through first kernel instruction. `lab1_5_requirement.md` additionally
requires chapter/module analysis, OS-principle mapping and unimplemented topics.
The delivery convention requires Markdown report, prompt history, genuine
screenshots and Git upload. Personal metadata may remain placeholders per the
user's explicit instruction. No grader score is required or invented.

## Evidence audit

- Report/source signatures agree: `kern_init` is `int(void)` with `noreturn`,
  `memset` takes `char c`, console putchar takes `unsigned char`, and `sbi_call`
  returns `uint64_t`. Existing SBI inline-assembly risk is documented, not fixed
  or claimed universally correct.
- Retained successful JSON has 32 checks, all true, with `passed=true`. Counts
  include repeated instruction-boundary/reach checks; they are not 32 distinct
  functional features. Six reset steps record actual opcodes and register effects.
- Reset PC 0x1000/M, firmware target 0x80000000, kernel 0x80200000/S/satp=0,
  stack 0x80201000..0x80203000 and BSS 0x80203070..0x80203070 match the report.
- The entire 12,400-byte raw image matches guest RAM at paused reset. Source
  and report distinguish preloading by QEMU from control handoff by OpenSBI.
- The empty original BSS call returns before a separately labeled synthetic
  64-byte clearing call. Guest bytes and caller-visible registers are restored;
  no claim of real nonempty BSS clearing is made. This is not normal unmodified
  boot and is supplemented by separate smoke evidence.
- Trap evidence records M privilege, mcause=9 and mepc equal to the observed
  console ecall address; subsequent cprintf return is S privilege. The script
  asserts privilege/cause and mepc matching the actual decoded ecall address.
- Independently rerun `sha256sum` and `cmp` on delivery and archived baseline
  raw images: both hashes are
  `404d805a3f95bc4a2432e0db04055635ec9d014f51a0fc3a324f1986704fb17a` and `cmp`
  exits zero. This proves binary equality for these artifacts/toolchain only.
- `uname -r` independently confirms 6.18.33.2-microsoft-standard-WSL2;
  `/etc/os-release` confirms Ubuntu 24.04.5 LTS. No QEMU process remained at audit.

## Independent focused failure probes

Exact command from the repository root:

```sh
uv run --locked python .temp/lab1-delivery-validation/test_delivery.py
```

One stdlib unittest runs five independent subcases, all passed (1.219 seconds).
The harness redirects verifier ROOT to a separate per-case directory, stubs
build/tool discovery, mocks debugger outcomes, but uses a real owned local
sleeping process for lifecycle assertions. These are host-orchestration checks,
not a second real-QEMU boot integration run. Full output is retained in
`.temp/lab1-delivery-validation/host-checks.log`.

| Failure condition | Expected and observed result |
| --- | --- |
| Recipe endpoint not the selected loopback port | Reject before child start; CLI 1, JSON false. |
| Debugger exits 7 despite passed JSON and boot banner | CLI 1 and JSON false; child reaped. |
| No boot banner despite passed JSON and debugger exit 0 | CLI 1, banner assertion false, child reaped. |
| Debugger timeout with partial stdout | CLI 1, useful error and exact partial output retained, child reaped. |
| QEMU child exits before debugger attachment | CLI 1 and explicit early-exit error; no debugger acceptance. |

The separate retained real-QEMU negative test reports CLI 1, debugger timeout,
and unchanged empty before/after emulator PID sets. It is reviewed evidence,
not rerun here because its default destination overwrites successful evidence.

## Boundaries and final packaging

No consequential implementation defect was reproduced in this bounded audit.
The port reservation has a documented release-to-bind race; it is not eliminated.
The nonempty real BSS branch was not exercised by this zero-BSS build. Firmware
internals, hardware operation, all printf formatting and future toolchains are
not verified by this lab. Git remote upload must be confirmed by the delivery
owner; local evidence is not remote submission.

Final local packaging recheck passed:

```sh
uv run --locked python .temp/lab1-delivery-validation/test_assets.py
identify report/images/*.png
```

- All 10 Markdown local links in report/report.md resolve; prompt.md and the
  evidence README have no unresolved Markdown links.
- All 16 evidence/panel SHA-256 entries and all five screenshot SHA-256 entries
  checked against manifest.json match.
  Seven core published logs/results/commands are byte-identical to the final
  `.cache/lab1` originals. Raw kernel digest matches the manifest.
- Successful published result still contains exactly 32 true checks. Trap
  assertion now explicitly compares mepc to the decoded ecall address.
- Five genuine PNG files decode through ImageMagick. Dimensions in report order
  are 1102x612, 1102x726, 1102x783, 1102x422 and 1102x954. Each corresponding
  committed display panel explicitly says RECORDED OUTPUT REPLAY. This audit
  checks format/dimensions/provenance, not an independent visual legibility
  review; the delivery owner separately performed visual inspection.
- Audit-history agent count was corrected to three. Metadata placeholders
  remain intentional, user-approved, and are not a failure.

Asset results are in `.temp/lab1-delivery-validation/asset-checks.json`.
The first asset harness run incorrectly imposed an arbitrary 500-pixel minimum
height and rejected the valid 422-pixel checks panel. That was a harness defect,
not a delivery defect: the requirement is valid, complete, readable screenshot
content, not a minimum pixel height. The corrected format/positive-dimension
check and independent ImageMagick decode both pass.

**Verdict:** Local lab exercise/report/evidence delivery accepted within the
stated bounds; no blocking defect found. Remote Git submission remains for the
owner to verify after commit/push. No official grader or comprehensive OS
correctness is claimed.
