# Initial Lab2 implementation and test delegations

These are the actual initial module instructions from the coordinator. They
complement the later four-part review/test prompts in `prompt.md`. Routine
progress and capacity-retry messages are not implementation specifications.

## Physical memory and Best-Fit

```text
Implement Lab2 physical page initialization and Best-Fit. Own ONLY code/kern/mm/pmm.c and best_fit_pmm.c (headers if essential), docs/lab2-memory-notes.md (English durable findings). Read docs/course/lab2 exercise and requirements and supplied tests. Preserve basic_check and best_fit_check byte-for-byte; preserve lab2 comments, leave YOUR CODE student placeholder unless actual student ID known (do not invent). Implement robust page_init bounded by actual DTB and supported mapping (PHYSICAL_MEMORY_END), overflow/alignment/reservation validation; select best_fit manager. Best-Fit sorted address free list, split/coalesce and correct nr_free. Research primary sources for technical uncertain facts; persist concise findings. Do not edit init.c/Makefile or tests. Other agents implementing Buddy/SLUB. Run focused compile/tests when possible, avoid make clean/shared QEMU races. No git commit. Report interfaces and risks.
```

## Buddy implementation

```text
Implement standalone Buddy System allocator satisfying Lab2 tasks. Own ONLY new code/kern/mm/buddy_pmm.c, buddy_pmm.h and docs/lab2-buddy-notes.md (English). Integrate via const struct pmm_manager buddy_pmm_manager (parent edits pmm/init/Makefile). Contract: orders power-of-two blocks, alloc_pages(n) rounds up to next power of two and records allocation order; free_pages(base,n) called with ORIGINAL requested n, verifies recorded order, returns full rounded block; document this. use existing struct Page without changing layout; can use ref/property flags carefully under exclusive ownership. init_memmap decomposes arbitrary base ranges into aligned blocks; support <=128 MiB default, declared max order enough supported mapping. No unbounded metadata. buddy.check self-tests or helper minimal; independent test agent writes extended tests. Preserve existing contracts Best-Fit. Parent will temporarily switch pmm_manager to buddy, initialize same free span via exported globals or delegate initialization by test chosen region. Tell parent how to run integration. Research primary Linux buddy/docs relevant, persist mechanism/constraints. Do not edit shared files/tests/Makefile/init.c. No git commit.
```

## SLUB implementation

```text
Implement simplified educational SLUB allocator Lab2. Own ONLY new code/kern/mm/slub.c, slub.h and docs/lab2-slub-notes.md English. Parent integrates boot/tests. Public API void slub_init(void), void *kmalloc(size_t), void kfree(void *), void slub_check(void) optional. Backed by current global alloc_pages/free_pages (Best-Fit by default), virtual address page2pa + va_pa_offset; support size classes e.g 16..2048 and direct page allocations for larger objects, 0 returns NULL, overflow returns NULL, alignment at least 16, full empty slabs returned to PMM. Keep ownership/metadata robust, avoid changing Page layout; can store self-describing headers in page and object metadata and use Page ref/property ONLY if contract safe (coordinate with buddy). Single hart no concurrency needed explicitly. Prefer simple slab size-class partial lists; do not overengineer production Linux clone. Research official Linux SLUB docs and relevant primary academic paper if useful, persist tradeoffs. No edit shared files/Makefile/init.c/tests. No git commit. Report actual API.
```

## Independent allocator tests

```text
Design and IMPLEMENT independent guest allocator tests for Lab2, owning ONLY new code/kern/mm/allocator_checks.c and allocator_checks.h plus docs/lab2-test-plan.md English. Read exercise/spec/tests and existing page interface. APIs from other agents: extern const struct pmm_manager buddy_pmm_manager, buddy alloc(n) power-of-two rounding and free(base,original n); SLUB void slub_init(), void *kmalloc(size_t), void kfree(void *), optional slub_check. Coordinate directly agents if needed. Parent will call allocator_checks() after pmm_init with Best-Fit active. Test Best-Fit via alloc/free deterministic mixed operations against small shadow map, nonoverlap, nr_free conservation, coalesce and actual smallest-fit choice with isolated allocation arena if necessary. Test Buddy without sacrificing global Best-Fit state: reserve aligned region using Best-Fit, temporary switch global manager to buddy and buddy.init/init_memmap on reserved Page span (note init_memmap may require SetPageReserved flags), test then restore Page state and original manager/free reserved span, avoid kernel corruption. SLUB payload patterns, cross-object nonalias, alignment, boundaries including 0, >page, varied order free, PMM baseline restoration, deterministic property-style mixed traces. Test output markers clear, asserts fail useful. Do not edit supplied basic_check/best_fit_check/test_lab2 or grading script. No production fixes. Don't run shared make clean. No git commit.
```
