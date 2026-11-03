# Independent Lab3 course-compliance review

Review date: 2026-11-03. Scope: the current `lab3` working tree, not only the
reported score. The reviewer did not modify kernel, test or report sources.

## Conclusion

The implementation meets the required Lab3 functional scope. No blocking code
or runtime defect was found. The teacher-cancelled user/kernel switching
Challenge is correctly absent. The report, prompts, screenshots and retained
execution output cover the required deliverables. The two documentation
completion items raised during review are now resolved. No outstanding
actionable implementation or report-content finding remains.

## Requirement coverage

| Obligation | Inspection and execution result |
| --- | --- |
| `RESTORE_ALL` mirrors `SAVE_ALL` | All writable integer registers are reloaded; status/EPC precede scratch-register restoration, and original SP is restored last. 288-byte layout matches `struct trapframe`. |
| `__alltraps` passes the frame to C and returns through `sret` | `a0=sp`, `jal trap`, then `__trapret`; no additional stack imbalance. Real GDB evidence confirms argument and original-SP delta. |
| Preserve supplied context contract | SAVE_ALL's body and `trap.h` are byte-identical to the branch skeleton. The local `sbadaddr` alias only updates the assembler spelling of CSR 0x143. |
| Sustained timer, counting, reporting, shutdown | Next event is armed before incrementing ticks; every 100 ticks is reported; ten reports invoke SBI shutdown. An independent normal boot exited successfully with exactly ten reports. |
| Illegal instruction and breakpoint recovery | Both categories diagnose the fault and advance EPC from actual 16/32-bit encoding. Extra real guest probes cover both widths in both categories. |
| Other causes preserve diagnostics | Known non-timer categories print their names; unknown interrupts print their cause and frame; other exceptions print the frame without fabricating recovery. Course fallback output is retained. |
| Lab2 dependency | Page initialization and Best-Fit are carried forward; Lab3's interrupt-protected wrappers remain. The normal boot completes the allocator self-check. |
| Unmodified course assessment | Course test and injector sources are unchanged. `grade.sh` changes only the QEMU boot option; points and matching rules are unchanged. Existing results are 35/35, 25/25 and 40/40. |
| Report content | Overall logic, module functions, specifications/prompts, observed integration problems and fixes, OS-principle mapping and unimplemented topics are present. |
| Delivery structure | `code/`, `report/report.md`, `report/prompt.md` and referenced images exist on `lab3`. |
| Team attribution | Three student members have explicit implementation and report responsibilities, with matching module-owner fields. Codex is listed as an AI tool, not a team member. |

The exercise names SHUTDOWN_AFTER but the skeleton omits its definition. Choosing
10 is documented consistently in implementation, specification and report;
no contradictory supplied numeric requirement was found.

## Findings and resolution

### Resolved P2 — Complete the prescribed pre/post-condition specification format

Location when raised: `report/prompt.md:52-58` (Module 2), plus the integration
specification at lines 78-81. The course prompt guidance at
`docs/course/lab0.5/3_prompt_structure.md:85-95` asks for Pre-Condition and
Post-Condition for each main function. Module 1 already followed that format,
but Module 2 initially provided only behavior paragraphs. The behavior was
correct; the gap is in the documented specification format, not kernel logic.

Remedy: provide explicit pre/post conditions separately for the two handlers,
including a valid kernel frame, readable handled instruction at EPC, timer
re-arming/counting/reporting and selective EPC advancement. Apply the same
format to the integration specification for consistency.

Status: resolved. The final prompt now distinguishes entry/restore pre/post
conditions, specifies them separately for both C handlers, and includes them
for integration. Its final section identifies this as a review-time
specification refinement. No kernel changes were needed.

### Resolved — Replace the AI-model placeholder

`report/report.md` originally left the model identity as an instruction to fill
it later. The parent recovered `gpt-6.1-sol` from the actual implementation-turn
metadata and the updated environment table now records it separately from
Codex desktop. The report-template field is complete.

## Independent verification

- Compared all 55 recorded final source fingerprints with current files: match.
- Compared SAVE_ALL, trap.h, course guest tests and injector against the branch
  skeleton: unchanged, except the outside-the-macro assembler name alias.
- Inspected original per-case grading output, normal/probe logs, verifier logic,
  real GDB output and screenshot references rather than relying on the total.
- Ran a fresh normal QEMU boot in `os-labs:lab3`: exit status 0, allocator check
  passed, exactly ten `100 ticks` reports. Output: `report/evidence/review-boot.log`.
- Ran the isolated extra guest separately: exit status 0, two breakpoint
  diagnostics, two illegal-instruction diagnostics, all four continuations
  passed, timer resumed at tick 200, and ten reports before normal shutdown.
  Output: `report/evidence/review-probe.log`.
- Did not rerun the full course grader: no changed kernel behavior or evidence
  invalidated the existing grading runs.

## Submission action, not a code finding

The course also requires uploading the completed branch. At review time these
changes remain uncommitted and unpushed. The technical deliverables are present;
the final commit/push is a separate remaining submission action and was not
performed by this review.
