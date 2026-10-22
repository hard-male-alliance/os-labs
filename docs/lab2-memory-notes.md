# Lab2 physical memory initialization and Best-Fit

## Contracts and representation

The local course exercise and memory-layout guide are authoritative for this
teaching kernel. `npage` is the exclusive **physical page number** upper bound,
not a count of RAM pages; descriptor index zero represents `nbase =
DRAM_BASE / PGSIZE`. `pages` is a page-aligned kernel virtual pointer immediately
after the linked image. Its physical allocation is reserved before exposing the
remaining complete pages through `init_memmap`.

`page_init` rejects missing/overflowing DTB ranges and ranges not containing the
kernel. Its supported upper bound is the smaller of the DTB end and
`PHYSICAL_MEMORY_END`, rounded down. It validates descriptor capacity before
writing any descriptor, initializes every descriptor field, marks the entire
array reserved, then registers only the rounded-up tail following metadata.
Firmware and the kernel remain reserved. A zero-sized free tail does not invoke
`init_memmap`; subsequent framework checks require enough free memory to run.

The boot mapping offset is `PHYSICAL_MEMORY_OFFSET`, not `KERNBASE - mem_base`:
`entry.S` fixes that mapping independently of the DTB range. Metadata sizing is
bounded by the fixed 128 MiB physical-memory ceiling, so descriptor multiplication
and alignment cannot approach integer overflow after range validation.

The Best-Fit manager owns one address-ordered intrusive list, whose heads alone
carry `PG_property`, and an exact total free-page count. Allocation scans for the
smallest sufficient block, choosing the lowest-address block on equal sizes.
Exact matches terminate the scan. Splitting retains a free tail in the old list
position. Initialization clears reserved metadata and uses the ordinary free
path, so adjacent initialization regions also coalesce. Release validates
non-overlap against both neighbours **before** changing state, then merges both
sides. This also detects a double-free inside an existing block, which checking
only `PG_property` would miss. Allocation and free are O(number of free blocks),
with additional O(n) descriptor initialization on release.

## Source context and limitations

- [Local exercise](course/lab2/lab2_2_1_exercise.md) defines the interface and
  explicitly forbids editing `basic_check` and `best_fit_check`.
- [Local physical-memory guide](course/lab2/lab2_3_5_2_search_phymem_layout.md)
  explicitly allows reclaiming the DTB after initial parsing. No later code may
  read `boot_dtb` contents under this contract.
- [Devicetree Specification, memory nodes](https://devicetree-specification.readthedocs.io/en/stable/devicenodes.html#memory-node)
  defines `reg` as address/size pairs and permits multiple regions. The supplied
  parser exposes only one pair; this implementation does not claim arbitrary
  hardware discovery or reserved-memory-map support.
- Supplied grading assumes that `npage` equals the uncapped DTB end. This agrees
  with the normal 128 MiB QEMU configuration and smaller supported RAM sizes;
  above 128 MiB the course mapping ceiling still caps managed memory correctly,
  but that particular grading assertion is not a general-capacity test.
- The allocator API permits freeing subranges, as required by provided tests;
  it therefore does not keep allocation extent ownership records. It rejects
  overlap with free ranges, not a semantically wrong free of an allocated range.
- Calls require exclusive single-hart access; there is no SMP synchronization.
  Reclaiming firmware or arbitrary DTB-reserved regions is not supported.

## Focused verification (2026-10-22)

`make -C code obj/kern/mm/pmm.o obj/kern/mm/best_fit_pmm.o` passes with the
project cross compiler and `-Werror`. Exact comparison of the supplied test
region against the pre-edit copy confirms both test bodies are byte-identical.

A host harness includes the **actual** `best_fit_pmm.c`, supplies PMM wrappers,
and first executes the unchanged framework tests. It then runs 50,000 operations
against a 4,096-page bitmap model, with 256 live slots, requests of 1–32 pages,
and deterministic LCG seed 88435. Every operation checks smallest-block selection
(and lowest-address ties), exact free count, address ordering, maximal
coalescence, no overlap, and complete allocated/free coverage. Releasing all
remaining allocations restores exactly 4,096 pages. Result: PASS. The reproducible
harness is `tests/best_fit_property.c`, invoked by `make property`. Integrated
kernel/QEMU and course grading are recorded separately in the verification note.

An initial build caught that this kernel has no `MIN` macro. The final code uses
an explicit bounded-end expression rather than adding a global utility macro.

## Four-part implementation review prompt

The following follow-up was applied to the actual implementation:

> [PROMPT] Review actual pmm.c and best_fit_pmm.c implementations; preserve provided tests and API. [RELY] dtb getters; PHYSICAL_MEMORY_OFFSET/END; nbase indexes DRAM_BASE; pmm_manager contract; course local documents. [GUARANTEE] page_init and complete best_fit_pmm_manager plus active selection. [SPECIFICATION] page_init validates nonzero supported DRAM range and overflow, page-aligns end placement, reserves firmware/kernel/metadata, frees only complete available pages, leaves no out-of-range descriptors; Best-Fit selects smallest sufficient block, exact n-page accounting, address-ordered insertion and two-sided coalescence, no supplied-test edits. Pre: boot DTB parsed, exclusive single-hart PMM. Post: byte layout and external API unchanged, focused objects compile. Document any remaining hidden risks. Parent handles integration.

The second pass found no additional defect. Student-ID placeholders remain
intact because no real student ID was supplied; a fabricated ID would break
submission provenance rather than complete it.
