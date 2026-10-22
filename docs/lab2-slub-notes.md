# Lab2 educational SLUB allocator

## Scope and API

`code/kern/mm/slub.c` implements a deliberately small, single-hart allocator on
`alloc_pages()` / `free_pages()`. It does not change `struct Page` or borrow its
reference count, property, flags, or linkage. Calls must be serialized; an
interrupt handler must not reenter it. Initialize after `pmm_init()` using
`slub_init()` (idempotent). `kmalloc()` also performs this local initialization.

- `void *kmalloc(size_t size)`: uninitialized storage, aligned to 16 bytes;
  zero, arithmetic overflow, and allocation failure return `NULL`.
- `void kfree(void *ptr)`: accepts `NULL` or an exact live allocation pointer.
  Invalid/interior/double frees are outside the API contract. Header, slot-bound,
  and allocation-bitmap assertions diagnose many such errors, but this is not a
  hardened arbitrary-pointer validator (a stale pointer may already be unmapped
  or its page reused).
- `void slub_check(void)`: deterministic in-kernel tests, requiring available PMM
  memory and no unrelated memory allocation during the check.

## Ownership and representation

Eight size classes hold objects of 16, 32, 64, 128, 256, 512, 1024, and 2048
bytes. Each small slab owns one physical page. Its first 96 bytes contain a
header on RV64: magic, intrusive partial-list node, free-object head, page
count, class index, capacity, live count, and a 256-bit allocation bitmap.
The remaining whole slots provide caller storage. For example the 16-byte
class has 250 slots, while the 2048-byte class has one slot.

Only slabs with free slots are on their size class's partial list. Full slabs
are unlisted and become partial on the first free. Each unused object stores
its next-free pointer in its own bytes. A live slot's bitmap bit is set; its
whole slot is caller-owned and never reused as allocator metadata. The final
free removes and invalidates the header before returning the page to PMM.
No empty slabs are cached, so completely releasing a workload restores the
original free-page count.

Requests above 2048 bytes use a contiguous PMM allocation, with the same header
in the first page and data immediately after its 16-byte-aligned end. The
header stores the exact allocated page count. The pointer always starts in
the first page, so rounding an exact allocation pointer down to a page recovers
the header for both paths. Rounded-size arithmetic is checked before addition;
requests exceeding all available free pages fail before calling PMM. A request
can still fail due to physical fragmentation, which is normal.

The ordinary small allocation/free path is constant-time; making a new slab
initializes its finite slot freelist. Page acquisition/release inherits the
selected PMM's complexity. A 2048-byte slab has significant internal waste;
this is an intentional simple size-class tradeoff, not a production efficiency
claim. No constructors, object coloring, redzones, NUMA, per-CPU caches,
concurrency, realloc, or cache-specific object types are implemented.

## Research basis and tradeoffs

The [upstream Linux SLUB source](https://github.com/torvalds/linux/blob/master/mm/slub.c)
explicitly documents partial slab lists, no ordinary full-slab list, moving a
full slab onto a partial list when its first object is freed, and reclaiming
empty slabs. These lifecycle rules are directly useful in this single-hart
lab. Linux's per-CPU fast paths, atomics, locks, and NUMA mechanisms address
workloads absent here and would add correctness risks without lab benefit.
The course permits a simplified two-layer allocator, so this module preserves
page-to-slab-to-object ownership rather than copying Linux's broader API.

[Bonwick's 1994 paper](https://www.usenix.org/conference/usenix-summer-1994-technical-conference/slab-allocator-object-caching-kernel)
provides the foundational object-cache model and explains retaining initialized
object state and coloring for cache utilization. This lab uses raw size-class
storage, not typed object construction or retained object state; it demonstrates
the slab partitioning mechanism rather than claiming the paper's complete
object-caching benefits. These sources were consulted on 2026-10-22; upstream
SLUB evolves, so its present concurrency architecture is not treated as a stable
lab requirement.

## Tests and integration

`slub_check()` checks zero and maximum-size failures; NULL free; all class
boundaries and one-/multi-page direct requests; alignment; complete requested
payload writes/reads; exact PMM page-count restoration; a 300-object trace
crossing a 16-byte slab boundary; full-to-partial reuse; coexistence of live and
freed objects; and reverse-order cleanup preserving surviving payloads.
The parent integrates invocation after PMM checks and owns independent mixed
trace, OOM, and user-journey validation.

Local compilation of `make -C code obj/kern/mm/slub.o` passed with the course
RV64 toolchain and `-Wall -Werror`. End-to-end QEMU evidence belongs in the
final experiment report after the shared kernel has been integrated; compilation
alone is not a runtime correctness claim.

## Actual follow-up specification prompt

The coordinator supplied this prompt during implementation; preserved verbatim:

> [PROMPT] Finish simplified SLUB object allocator with two-level page/object management. [RELY] existing alloc_pages/free_pages and page/address mapping; single hart; no Page layout changes. [GUARANTEE] slub_init, kmalloc(size), kfree(ptr), slub_check. [SPECIFICATION] Pre: PMM initialized, slub_init once before allocations. kmalloc(0)/impossible sizes return NULL; all successes at least16-byte aligned and nonoverlapping with requested-byte payload safe; size classes cover small requests and page-backed large fallback. kfree(NULL) no-op; freeing valid live allocation restores ownership and empty slab returns all backing pages. Allocation failure preserves existing live allocations. Keep bounded metadata and explicit misuse assertions; no multicore/per-CPU performance claims.
