#include <allocator_checks.h>
#include <best_fit_pmm.h>
#include <buddy_pmm.h>
#include <pmm.h>
#include <slub.h>
#include <stdio.h>

#define ARENA_PAGES 64
#define PAGE_SLOTS 24
#define OBJECT_SLOTS 48

struct page_allocation {
    struct Page *page;
    size_t requested;
    size_t occupied;
};

struct object_allocation {
    unsigned char *address;
    size_t size;
    unsigned char pattern;
};

static unsigned int next_random(unsigned int *seed) {
    *seed = *seed * 1664525U + 1013904223U;
    return *seed;
}

static size_t rounded_pages(size_t requested) {
    size_t n = 1;
    while (n < requested) {
        n <<= 1;
    }
    return n;
}

static size_t smallest_hole(unsigned char *used, size_t requested) {
    size_t smallest = ARENA_PAGES + 1;
    size_t start = 0;
    while (start < ARENA_PAGES) {
        if (used[start]) {
            start++;
            continue;
        }
        size_t end = start;
        while (end < ARENA_PAGES && !used[end]) {
            end++;
        }
        if (end - start >= requested && end - start < smallest) {
            smallest = end - start;
        }
        start = end;
    }
    return smallest;
}

static bool aligned_hole(struct Page *arena, unsigned char *used, size_t n) {
    for (size_t start = 0; start + n <= ARENA_PAGES; start++) {
        if (page2ppn(arena + start) % n != 0) {
            continue;
        }
        size_t length = 0;
        while (length < n && !used[start + length]) {
            length++;
        }
        if (length == n) {
            return 1;
        }
    }
    return 0;
}

static void release_slot(struct page_allocation *slot, struct Page *arena,
                         unsigned char *used) {
    size_t offset = slot->page - arena;
    for (size_t i = 0; i < slot->occupied; i++) {
        assert(used[offset + i]);
        used[offset + i] = 0;
    }
    free_pages(slot->page, slot->requested);
    slot->page = NULL;
}

/* The bitmap is an independent ownership oracle, not allocator metadata. */
static void page_trace(struct Page *arena, bool buddy, unsigned int seed) {
    struct page_allocation slots[PAGE_SLOTS] = {{0}};
    unsigned char used[ARENA_PAGES] = {0};
    size_t occupied = 0;
    for (size_t step = 0; step < 512; step++) {
        struct page_allocation *slot = &slots[next_random(&seed) % PAGE_SLOTS];
        if (slot->page != NULL) {
            occupied -= slot->occupied;
            release_slot(slot, arena, used);
            assert(nr_free_pages() == ARENA_PAGES - occupied);
            continue;
        }
        size_t requested = 1 + next_random(&seed) % 11;
        size_t n = buddy ? rounded_pages(requested) : requested;
        size_t hole = smallest_hole(used, n);
        struct Page *page = alloc_pages(requested);
        if (page == NULL) {
            assert(buddy ? !aligned_hole(arena, used, n) : hole == ARENA_PAGES + 1);
            assert(nr_free_pages() == ARENA_PAGES - occupied);
            continue;
        }
        assert(page >= arena && page + n <= arena + ARENA_PAGES);
        size_t offset = page - arena;
        if (buddy) {
            assert(page2ppn(page) % n == 0);
        } else {
            assert(offset == 0 || used[offset - 1]);
            size_t end = offset;
            while (end < ARENA_PAGES && !used[end]) {
                end++;
            }
            assert(end - offset == hole);
        }
        for (size_t i = 0; i < n; i++) {
            assert(!used[offset + i]);
            used[offset + i] = 1;
        }
        slot->page = page;
        slot->requested = requested;
        slot->occupied = n;
        occupied += n;
        assert(nr_free_pages() == ARENA_PAGES - occupied);
    }
    for (size_t i = 0; i < PAGE_SLOTS; i++) {
        if (slots[i].page != NULL) {
            release_slot(&slots[i], arena, used);
        }
    }
    assert(nr_free_pages() == ARENA_PAGES);
    struct Page *whole = alloc_pages(ARENA_PAGES);
    assert(whole == arena);
    assert(alloc_page() == NULL);
    free_pages(whole, ARENA_PAGES);
}

static void check_best_fit(void) {
    size_t baseline = nr_free_pages();
    assert(baseline > ARENA_PAGES);
    /* At boot, page_init registers one contiguous free range. Holding its tail
     * isolates the oracle arena without reaching into the manager's lists. */
    struct Page *all = alloc_pages(baseline);
    assert(all != NULL && nr_free_pages() == 0);
    free_pages(all, 10);
    free_pages(all + 11, 4);
    struct Page *smallest = alloc_pages(3);
    assert(smallest == all + 11);
    free_pages(smallest, 3);
    struct Page *large = alloc_pages(10);
    struct Page *small = alloc_pages(4);
    assert(large == all && small == all + 11);
    free_pages(all, ARENA_PAGES);
    page_trace(all, 0, 0xB357F17U);
    page_trace(all, 0, 0x12345678U);
    assert(alloc_pages(ARENA_PAGES + 1) == NULL);
    struct Page *arena = alloc_pages(ARENA_PAGES);
    assert(arena == all);
    free_pages(all, baseline);
    assert(nr_free_pages() == baseline);
    cprintf("allocator checks: best-fit shadow traces passed\n");
}

static void check_buddy_regions(struct Page *arena) {
    struct Page *live[ARENA_PAGES];
    unsigned char seen[ARENA_PAGES] = {0};
    pmm_manager->init();
    for (size_t i = 0; i < ARENA_PAGES; i++) {
        arena[i].flags = 0;
        arena[i].property = 0;
        list_init(&arena[i].page_link);
        SetPageReserved(arena + i);
    }
    // Deliberately unaligned, disjoint registrations must not bridge the hole.
    pmm_manager->init_memmap(arena + 1, 31);
    pmm_manager->init_memmap(arena + 40, 24);
    assert(nr_free_pages() == 55);
    assert(alloc_pages(32) == NULL);
    for (size_t i = 0; i < 55; i++) {
        live[i] = alloc_page();
        assert(live[i] != NULL);
        size_t offset = live[i] - arena;
        assert((offset >= 1 && offset < 32) || (offset >= 40 && offset < 64));
        assert(!seen[offset]);
        seen[offset] = 1;
    }
    assert(alloc_page() == NULL && nr_free_pages() == 0);
    for (size_t i = 0; i < 55; i++) {
        free_page(live[(i * 17) % 55]);
    }
    assert(nr_free_pages() == 55);
    // Filling the missing registrations must make the whole block coalesce.
    pmm_manager->init_memmap(arena + 32, 8);
    assert(nr_free_pages() == 63 && alloc_pages(64) == NULL);
    pmm_manager->init_memmap(arena, 1);
    assert(nr_free_pages() == ARENA_PAGES);
    struct Page *whole = alloc_pages(ARENA_PAGES);
    assert(whole == arena && nr_free_pages() == 0);
    free_pages(whole, ARENA_PAGES);
    pmm_manager->check();
}

static void check_buddy(void) {
    size_t baseline = nr_free_pages();
    struct Page *reserved = alloc_pages(2 * ARENA_PAGES);
    assert(reserved != NULL);
    size_t padding = (ARENA_PAGES - page2ppn(reserved) % ARENA_PAGES) % ARENA_PAGES;
    struct Page *arena = reserved + padding;
    const struct pmm_manager *saved = pmm_manager;
    for (size_t i = 0; i < ARENA_PAGES; i++) {
        SetPageReserved(arena + i);
    }
    pmm_manager = &buddy_pmm_manager;
    pmm_manager->init();
    pmm_manager->init_memmap(arena, ARENA_PAGES);
    assert(nr_free_pages() == ARENA_PAGES);
    assert(alloc_pages(0) == NULL);
    assert(alloc_pages((size_t)-1) == NULL);
    assert(alloc_pages(ARENA_PAGES + 1) == NULL);
    struct Page *rounded = alloc_pages(3);
    assert(rounded != NULL && page2ppn(rounded) % 4 == 0);
    assert(nr_free_pages() == ARENA_PAGES - 4);
    free_pages(rounded, 3);
    page_trace(arena, 1, 0xB0DD1234U);
    page_trace(arena, 1, 0xC0A1E5CEU);
    check_buddy_regions(arena);
    pmm_manager = saved;
    /* The arena descriptors belong to Best-Fit again; discard Buddy-private
     * flags only after its complete ownership/accounting check has passed. */
    for (size_t i = 0; i < 2 * ARENA_PAGES; i++) {
        reserved[i].flags = 0;
        reserved[i].property = 0;
        set_page_ref(reserved + i, 0);
        list_init(&reserved[i].page_link);
    }
    free_pages(reserved, 2 * ARENA_PAGES);
    assert(nr_free_pages() == baseline);
    cprintf("allocator checks: buddy rounding, shadow traces and coalescing passed\n");
}

static void verify_objects(struct object_allocation *slots) {
    for (size_t i = 0; i < OBJECT_SLOTS; i++) {
        struct object_allocation *a = &slots[i];
        if (a->address == NULL) {
            continue;
        }
        for (size_t k = 0; k < a->size; k++) {
            assert(a->address[k] == (unsigned char)(a->pattern + k));
        }
        for (size_t j = i + 1; j < OBJECT_SLOTS; j++) {
            struct object_allocation *b = &slots[j];
            assert(b->address == NULL ||
                   (uintptr_t)a->address + a->size <= (uintptr_t)b->address ||
                   (uintptr_t)b->address + b->size <= (uintptr_t)a->address);
        }
    }
}

static void fill_object(struct object_allocation *slot, size_t size,
                        unsigned char pattern) {
    slot->address = kmalloc(size);
    slot->size = size;
    slot->pattern = pattern;
    assert(slot->address != NULL);
    assert((uintptr_t)slot->address % 16 == 0);
    for (size_t k = 0; k < size; k++) {
        slot->address[k] = (unsigned char)(pattern + k);
    }
}

static void check_slab_turnover(void) {
    static unsigned char *objects[512];
    size_t baseline = nr_free_pages();
    for (size_t i = 0; i < 512; i++) {
        objects[i] = kmalloc(16);
        assert(objects[i] != NULL);
        for (size_t k = 0; k < 16; k++) {
            objects[i][k] = (unsigned char)(i + k);
        }
    }
    assert(nr_free_pages() <= baseline - 2);
    for (size_t i = 0; i < 512; i += 2) {
        kfree(objects[i]);
    }
    for (size_t i = 1; i < 512; i += 2) {
        for (size_t k = 0; k < 16; k++) {
            assert(objects[i][k] == (unsigned char)(i + k));
        }
        kfree(objects[i]);
    }
    assert(nr_free_pages() == baseline);
}

static void check_slub(void) {
    static const size_t sizes[] = {1, 15, 16, 17, 31, 32, 33, 63, 64, 65,
        127, 128, 129, 255, 256, 257, 511, 512, 513, 1023, 1024,
        1025, 2047, 2048, 2049, 4095, 4096, 4097, 8193};
    struct object_allocation slots[OBJECT_SLOTS] = {{0}};
    size_t baseline = nr_free_pages();
    slub_init();
    assert(kmalloc(0) == NULL);
    assert(kmalloc((size_t)-1) == NULL);
    kfree(NULL);
    for (size_t i = 0; i < OBJECT_SLOTS; i++) {
        fill_object(&slots[i], sizes[i % (sizeof(sizes) / sizeof(sizes[0]))], i);
    }
    verify_objects(slots);
    unsigned int seed = 0x51AB2026U;
    for (size_t step = 0; step < 256; step++) {
        size_t i = next_random(&seed) % OBJECT_SLOTS;
        verify_objects(slots);
        if (slots[i].address != NULL) {
            kfree(slots[i].address);
            slots[i].address = NULL;
            continue;
        }
        size_t size = sizes[next_random(&seed) % (sizeof(sizes) / sizeof(sizes[0]))];
        fill_object(&slots[i], size, step);
    }
    verify_objects(slots);
    /* An odd stride permutes all slots, freeing unlike allocation order. */
    for (size_t i = 0; i < OBJECT_SLOTS; i++) {
        size_t index = (i * 13) % OBJECT_SLOTS;
        kfree(slots[index].address);
        slots[index].address = NULL;
    }
    assert(nr_free_pages() == baseline);
    check_slab_turnover();
    struct Page *held = alloc_pages(baseline);
    assert(held != NULL);
    assert(kmalloc(16) == NULL);
    assert(kmalloc(4097) == NULL);
    assert(nr_free_pages() == 0);
    free_pages(held, baseline);
    assert(nr_free_pages() == baseline);
    cprintf("allocator checks: slub boundaries, payloads, nonalias and reclamation passed\n");
}

void allocator_checks(void) {
    assert(pmm_manager == &best_fit_pmm_manager);
    check_best_fit();
    check_buddy();
    check_slub();
    cprintf("allocator checks: final free pages = %lu\n", nr_free_pages());
    cprintf("allocator checks: all passed\n");
}
