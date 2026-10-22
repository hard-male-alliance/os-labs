# Lab2 independent implementation review

Reviewed on 2026-10-22 against the local course exercise, existing PMM API,
and the integration working tree. This review did not modify production code.

## Decision

**No blocking correctness defect found in the supported single-hart QEMU/OpenSBI
configuration.** The implemented algorithms fulfill the exercise's intended
contracts. Both initially identified coverage improvements have been resolved
and re-reviewed. The remaining observations are explicitly bounded platform
limitations, not demands for an unrelated production-kernel redesign.

## Scope and checks

Inspected `pmm.c`, `best_fit_pmm.c`, `buddy_pmm.c`, `slub.c`,
`allocator_checks.c`, `init.c`, `entry.S`, the supplied DTB parser, memory-layout
constants, and the local exercise. Reused the implementation research in
`lab2-memory-notes.md`, `lab2-buddy-notes.md`, and `lab2-slub-notes.md` rather than
repeating its upstream-source survey.

The existing host executable `.temp/lab2-pmm/property` was rerun successfully:
unchanged framework checks plus 50,000 bitmap-model operations passed. Its
source includes the actual Best-Fit implementation. `git diff --check` passed.
End-to-end guest and GDB evidence is recorded separately in
`lab2-verification.md`; this review does not relabel another agent's run as its
own execution.

| Component | Main invariant assessed | Assessment |
| --- | --- | --- |
| PMM initialization | Descriptor capacity checked before writes; firmware/image/metadata stay reserved; only complete pages become free | Consistent with the supplied one-range DTB interface and fixed management ceiling |
| Best-Fit | Sorted nonoverlapping maximal free blocks; smallest sufficient selection; exact counts | Split placement and both-side coalescing preserve invariants; valid subrange frees remain compatible with supplied tests |
| Buddy | Physical-PFN alignment; only same-order registered buddies merge; rounded accounting | Ownership and allocated-head bits have consistent lifetimes; original-request recording makes non-power-of-two release unambiguous |
| SLUB | Live bytes belong only to callers; partial-list membership matches availability; empty slab returns all backing pages | Small and large paths preserve these invariants; original page request is retained for Buddy-compatible release |
| Boot | Enable Sv39 without losing the physical instruction-fetch mapping; remove identity mapping after high jump | Temporary root entry 2 and retained high entry 511 implement that transition coherently |
| Test isolation | Best-Fit retains ownership outside Buddy arena; all descriptors restored before returning arena | Handoff clears Buddy-private metadata only after full reclamation, without corrupting saved Best-Fit lists |

## Coverage follow-up: both observations resolved

The initial review identified that Buddy NULL results during randomized traces
were not independently justified, and that initialization coverage used only
one aligned contiguous span. Both gaps have now been addressed in
`code/kern/mm/allocator_checks.c`; the follow-up test code was independently
re-reviewed and is sound.

- `aligned_hole()` scans the independent ownership bitmap and checks physical
  PFN alignment for every candidate rounded-size block. The Buddy NULL branch
  now asserts that no complete aligned free span exists. The test requests are
  bounded well below the supported maximum order, and this fully registered
  arena is maximally coalesced, so the predicate is a valid completeness oracle
  rather than an assumption about internal list placement.
- `check_buddy_regions()` discards only an already completely free isolated
  Buddy arena, reinitializes its descriptors as reserved, and registers the
  unaligned/disjoint spans `[1,32)` and `[40,64)`. It checks the exact 55-page
  total, failure of an impossible 32-page request, unique membership of every
  one-page allocation in those two spans, and exhaustion after 55 allocations.
- Release order `(i * 17) % 55` is a genuine permutation because 17 and 55 are
  coprime. Every allocation is therefore returned exactly once, and the
  restored count is checked before adding memory.
- Registering `[32,40)` restores 63 pages but still cannot satisfy an aligned
  64-page request. Registering the last missing page `[0,1)` must then coalesce
  the entire arena into the original 64-page block. Allocation, exact address,
  zero remaining pages, release, and the built-in Buddy invariant check all run
  before control restores Best-Fit ownership.

No new correctness issue was found in these changes. The parent also reports
that the now-persistent `tests/best_fit_property.c` passes 50,000 operations,
including an ASan/UBSan run with leak detection disabled (the harness allocates
no heap). These latter executions belong to the parent; the earlier direct
reviewer-run harness evidence is retained above.

## Accepted limits and compatibility conclusions

- `page_init()` validates the **extracted physical memory range**, not every
  byte of an arbitrary malformed DTB. The unchanged `dtb.c` parser trusts
  offsets, strings, and token lengths after checking magic. A malicious or
  truncated firmware blob could fault before `page_init()` runs. This is a
  pre-existing trusted-firmware limitation, not introduced by the allocator
  change; do not describe the kernel as accepting hostile arbitrary DTBs.
- The fixed `PHYSICAL_MEMORY_END` ceiling is an explicit course management
  boundary, even though the boot superpage maps a larger address range. Above
  128 MiB RAM, cap management rather than satisfy a grader assertion that
  incorrectly equates managed pages with the entire DTB range. Default course
  RAM and the smaller tested configurations retain the intended contract.
- Buddy is an additional manager with documented rounded allocation and
  original-size release, not a transparent substitute for Best-Fit tests that
  free allocation subranges. Existing users continue to use Best-Fit by
  default; no existing subrange-free contract is silently changed.
- SLUB invalid, stale, or interior pointers are outside `kfree`'s API contract.
  Its useful assertions are not a hardened arbitrary-pointer validator. Valid
  live pointers and `kfree(NULL)` follow consistent lifetimes.
- PMM/SLUB require serialized single-hart access. No locks are necessary for the
  current boot-time noninterrupt workload; enabling concurrent callers would
  require a separate synchronization change.
- Very small RAM configurations may fail framework tests because the tests
  require working memory. This is distinct from passing a zero-length range to
  `init_memmap`: initialization correctly avoids that call.

No student ID or member identity was invented; remaining identity placeholders
are submission metadata requiring the user's real information.
