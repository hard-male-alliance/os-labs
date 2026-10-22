# Lab2 Buddy allocator

## Integration and ownership

`code/kern/mm/buddy_pmm.c` exports `buddy_pmm_manager` through the existing
`pmm_manager` interface. Kernel source discovery already includes the new C
file. The default manager remains the parent's choice (Best-Fit for the course
checks). To exercise Buddy in isolation, reserve a contiguous region from the
active allocator, save `pmm_manager`, mark only that region's descriptors
reserved, select Buddy, call `init()` and `init_memmap()`, run checks, restore the
original manager and descriptors, then free the original reservation.
Never initialize two allocators over the same live pages.

The allocator is single-CPU/serialized, matching the current PMM framework;
callers must provide synchronization before introducing concurrent use. It
adds no `struct Page` members and allocates no auxiliary memory. State consists
of 16 intrusive list heads and one free-page counter. On owned pages:

- `PG_property` marks a free block head; `property` then stores its order.
- Flags bit 2 marks an allocated block head, bit 3 marks Buddy-owned pages.
- Flags bits 8..12 record an allocated block's order; its `property` records the
  exact original request size. Allocated page references start at zero.
- Reserved pages are never inserted; interior pages have neither head flag.

These private bits have meaning only while Buddy owns the descriptors. The
allocator must not share descriptor ownership with another manager.

## Algorithm and contract

An order-*k* block contains `2^k` pages, physically aligned to that size. The
buddy's absolute physical page number is `pfn XOR (1 << k)`, not an offset
relative to the first available page. This allows unaligned initial ranges
without losing pages or merging across holes. `init_memmap()` decomposes an
arbitrary reserved range into aligned blocks, releasing each through the
normal coalescing path. It can register multiple disjoint ranges, or adjacent
ranges that then coalesce. Descriptor bounds and the owned/free/order checks
prevent merging across excluded memory.

Allocation selects the first nonempty order at or above the rounded request,
removes one block, and repeatedly splits it, retaining one half and inserting
the other. Release repeatedly removes its free same-order buddy and merges
upward. Both paths take at most `BUDDY_MAX_ORDER + 1` order steps; release also
resets the returned block's descriptors, so total release work is O(2^order).
Initialization is linear in the number of registered pages.

`alloc_pages(n)` reserves `2^ceil(log2(n))` pages. `free_pages(base,n)` requires
the **original request size** and allocation head, not the rounded size and not
a subrange. A request for three pages consumes four; freeing `(base,3)` returns
all four. The recorded request and allocation flag assert against wrong-size,
interior-pointer and duplicate frees. Requests above `2^15` pages fail without
state changes, including zero and overflow-sized allocation requests. Invalid
free requests (including zero) assert before modifying state.
Order 15 spans 128 MiB at 4 KiB/page, sufficient for the supported QEMU mapping.
Larger physical regions can still be registered as several maximum-order
blocks, but cannot satisfy an individual request larger than that cap.

The different rounded-allocation/free contract means the supplied First-Fit
and Best-Fit tests (which deliberately free subranges) must not be reused as
Buddy tests. This is a separate implementation, not a change to their behavior.

## Verification

The built-in `check()` validates physical alignment, free-head tags, order,
complete coalescing and free-list accounting, then allocates 1, 3 and 17 pages,
checks rounded accounting, releases them in mixed order, and verifies full
recovery. It expects a normal initialized region with sufficient free blocks
(for isolated testing, an aligned 64-page region is sufficient).

A standalone target-toolchain compilation completed with `-Wall -Werror` on
2026-10-22; guest integration and extended allocation traces are handled by the
independent allocator tests. See the final experiment notes for actual guest
results rather than interpreting compilation as runtime validation.

## Production and research connections

The per-order list, splitting, and XOR buddy/coalescing mechanism follow the
classic binary Buddy structure described in [Linux's physical page allocation
chapter](https://www.kernel.org/doc/gorman/html/understand/understand009.html).
The current [upstream Linux page allocator](https://github.com/torvalds/linux/blob/master/mm/page_alloc.c)
also explicitly associates free block heads with orders. Linux adds zones,
concurrency, per-CPU caching and migration types; none are warranted by this
single-core teaching workload. Our original-request recording adapts the
course's size-based interface rather than requiring Linux's caller-supplied
order interface.

This design exposes the central tradeoff: merging is local and bounded, but
rounding induces internal fragmentation. For an n-page request, the reserved
count is between n and 2n-1. Equal-order total free capacity alone does not
ensure a contiguous higher-order allocation. The object allocator above Buddy
should handle small objects rather than attempting to remove that tradeoff by
adding special cases to page allocation.

## Actual course-format follow-up prompt

The parent supplied this final design-review prompt during implementation:

```text
[PROMPT] Review and finish buddy_pmm.c/h, preserve existing Page/API. [RELY] pmm_manager, Page flags/property/list link, physical PFNs, PGSIZE, exclusive reserved input span. [GUARANTEE] buddy_pmm_manager init/init_memmap/alloc_pages/free_pages/nr_free_pages/check. [SPECIFICATION] Pre: single hart and reserved in-range pages on init_memmap; partition arbitrary spans into maximal physically aligned 2^order blocks. alloc(n) returns rounded power-of-two block or NULL without state change; free(base,original_n) validates allocation and returns exactly rounded capacity, recursively merges only true same-order free buddies within registered memory. Reject zero/overflow requests safely. Preserve Page layout and independent Best-Fit ownership. Post: non-overlap, free count conservation, full reclamation. Include tests and limitations in note, no concurrency claims.
```

The review added explicit `alloc_pages(0)` and overflow-sized request rejection
with no state mutation, and built-in checks for both. Invalid frees remain
assertion failures, rather than silent leaks or partial state mutation.
