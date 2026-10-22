# Independent Lab2 allocator verification

## Scope and ownership

`code/kern/mm/allocator_checks.c` exercises public allocation APIs after
`pmm_init()` with Best-Fit active. It does not alter the supplied
`basic_check()`, `best_fit_check()`, course test source, or grader. Assertions
check ownership and accounting independently of allocator free-list metadata.
The tests are bounded, deterministic, and run inside the real RISC-V guest.

## Test-generation prompts

The coordinator supplied these separate prompts for the two extension tasks.
They are preserved verbatim rather than reconstructed from implementation:

### Buddy test prompt

> [PROMPT] Generate independent Buddy tests in allocator_checks.c. [RELY] buddy_pmm_manager rounds n to power2; free original n; isolated reserved physical-PFN-aligned arena. [GUARANTEE] Buddy test invoked from allocator_checks. [SPECIFICATION] Assert alignment/nonoverlap, split/coalesce, rounded capacity accounting, exhaustion/zero/overflow, deterministic mixed trace and full baseline restoration; never mutate original supplied tests or leave global manager changed.

### SLUB test prompt

> [PROMPT] Generate independent SLUB tests. [RELY] slub_init/kmalloc/kfree and current PMM; 16-byte alignment. [GUARANTEE] SLUB test invoked from allocator_checks. [SPECIFICATION] Boundary sizes including0,16,17,2048,2049,4096,multipage; patterns across all requested payload bytes; nonalias of live objects; partial/full/empty transitions, deterministic mixed trace, overflow failure and exact PMM reclamation; kfree(NULL) no-op. Keep tests bounded and source-level independent of implementation metadata.

## Assertions and discrimination

| Test | Setup and oracle | What a failure means |
| --- | --- | --- |
| Best-Fit policy | Two isolated holes of 10 and 4 pages; requesting 3 must use the 4-page hole | Distinguishes Best-Fit from address-ordered First-Fit |
| Best-Fit mixed traces | A 64-page ownership bitmap, 24 live allocation slots, two fixed seeds, 512 operations per seed | Detects overlap, allocation outside arena, missed fitting holes, wrong minimum-fit choice, and lost/free page accounting |
| Best-Fit coalescing | After all trace allocations are freed, allocate the entire arena | Detects missing coalescing even when free-page totals look correct |
| Buddy rounding | Allocate 3 pages, require a 4-page capacity debit and 4-page physical alignment; free with original request 3 | Detects unrounded accounting or asymmetric allocation/free behavior |
| Buddy mixed traces | Same ownership bitmap using rounded occupied lengths and physical-page alignment | Detects overlap, out-of-bounds allocations, wrong alignment and rounded capacity accounting |
| Buddy exhaustion | 65-page, zero and maximal size requests in a 64-page arena; an extra page after full-arena allocation | Checks nonmutating rejection and exhaustion |
| Buddy coalescing | Full 64-page allocation after each trace | Requires recursive buddy reunion, not only a correct free count |
| Buddy unaligned/disjoint registration | Register 31 pages at offset 1 and 24 pages at offset 40, exhaust 55 unique pages; fill 8-page gap then missing first page | Prevents cross-hole allocation, validates arbitrary registration boundaries and complete 64-page coalescing |
| SLUB boundaries | Sizes 1,15,16,17,...,2047,2048,2049,4095,4096,4097,8193 plus zero and maximal size | Covers class boundaries, page boundaries, large-object fallback, alignment, and overflow rejection |
| SLUB live ownership | Distinct patterns over every requested byte; pairwise live-range nonoverlap | Detects aliasing, premature reuse, payload corruption, and header/payload overlap |
| SLUB mixed trace | 48 slots, 256 operations, fixed seed, validate all live payloads before every operation | Covers interleaved class allocation/release and continued validity of unrelated objects |
| SLUB turnover | 512 concurrent 16-byte objects, release evens, verify odds, then release odds | Requires multiple pages, full-to-partial-to-empty transitions and preservation of survivors |
| SLUB OOM | Hold every currently free page; small and multi-page allocations must fail | Verifies ordinary PMM exhaustion without weakening successful-allocation assertions |
| Complete reclamation | Exact pre/post PMM count around every suite | Detects persistent page leaks, over-free, or failure to return empty slabs |

The property-style trace uses an LCG (`1664525*x + 1013904223`, modulo
32 bits) with recorded source seeds. Its independent bitmap represents
requested ownership only, never reads allocator lists or order flags. For
Best-Fit it derives the smallest adequate contiguous hole from the bitmap
and accepts any tied minimum. Buddy may correctly fail despite a large
unaligned contiguous hole; an independent bitmap scan now enumerates every
physically aligned candidate of the rounded size before accepting NULL. This
catches unnecessary failures without reading Buddy metadata. Trace cleanup
checks full coalescing.

## State isolation and safety

At this boot stage `page_init()` registers one contiguous free range.
Best-Fit temporarily holds this complete range, releases controlled holes,
and then exposes a 64-page arena. The tail remains allocated, so ordinary
public APIs suffice to isolate the test without touching private lists.
After the oracle run the whole original range is returned in one operation.
This boot-time contiguity precondition is explicit and asserted.

Buddy reserves 128 pages through Best-Fit and picks a physical-PFN-aligned
64-page subrange. Only that subrange is registered with Buddy. On successful
completion every Buddy allocation has been returned, the original manager
pointer is restored, reserved descriptors are cleared of Buddy-private state,
and the original 128 pages are freed through Best-Fit. No live kernel-owned
page is registered, freed, or written as payload. SLUB then runs under the
restored Best-Fit manager and returns all of its backing pages.

## Reproduction and result interpretation

The normal integration workflow is `make`, then `make qemu`. Expect:

```text
allocator checks: best-fit shadow traces passed
allocator checks: buddy rounding, shadow traces and coalescing passed
allocator checks: slub boundaries, payloads, nonalias and reclamation passed
allocator checks: final free pages = <post-init baseline>
allocator checks: all passed
```

A panic with an assertion source line is a failure, even if earlier markers
were printed. A successful compile alone is not acceptance. Use the RAM-matrix
and GDB workflow maintained by the verification agent to confirm real boot,
RAM sizing, returned baseline, and continued kernel operation after tests.

Local source validation on 2026-10-22:

```sh
riscv64-unknown-elf-gcc -mcmodel=medany -std=gnu99 -Wno-unused -Werror \
  -fno-builtin -Wall -O2 -nostdinc -fno-stack-protector \
  -ffunction-sections -fdata-sections -g -Icode/libs -Icode/kern/debug \
  -Icode/kern/driver -Icode/kern/mm -Icode/kern/arch \
  -fsyntax-only code/kern/mm/allocator_checks.c
```

Result: passed with the project compiler and warning policy. Runtime results
belong to the integrated verification run; this syntax check does not claim
the guest has booted.
