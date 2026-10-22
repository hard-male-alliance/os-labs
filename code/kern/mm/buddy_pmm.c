#include <buddy_pmm.h>
#include <stdio.h>

#define BUDDY_ALLOCATED (1UL << 2)
#define BUDDY_MANAGED (1UL << 3)
#define BUDDY_ORDER_SHIFT 8
#define BUDDY_ORDER_MASK (0x1FUL << BUDDY_ORDER_SHIFT)

static list_entry_t free_lists[BUDDY_MAX_ORDER + 1];
static size_t nr_free;

static size_t
order_size(unsigned order) {
    return (size_t)1 << order;
}

static unsigned
request_order(size_t n) {
    unsigned order = 0;
    while (order < BUDDY_MAX_ORDER && order_size(order) < n) {
        order++;
    }
    return order;
}

static struct Page *
find_buddy(struct Page *page, unsigned order) {
    size_t ppn = page2ppn(page) ^ order_size(order);
    if (ppn < nbase || ppn >= npage) {
        return NULL;
    }
    struct Page *buddy = pages + (ppn - nbase);
    if (!(buddy->flags & BUDDY_MANAGED) || !PageProperty(buddy) ||
        buddy->property != order) {
        return NULL;
    }
    return buddy;
}

static void
add_block(struct Page *page, unsigned order) {
    page->property = order;
    SetPageProperty(page);
    list_add(&free_lists[order], &page->page_link);
}

static void
release_block(struct Page *page, unsigned order) {
    while (order < BUDDY_MAX_ORDER) {
        struct Page *buddy = find_buddy(page, order);
        if (buddy == NULL) {
            break;
        }
        list_del(&buddy->page_link);
        ClearPageProperty(buddy);
        if (buddy < page) {
            page = buddy;
        }
        order++;
    }
    add_block(page, order);
}

static void
buddy_init(void) {
    for (unsigned order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_init(&free_lists[order]);
    }
    nr_free = 0;
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    assert(base >= pages && base < pages + (npage - nbase));
    assert(n <= (size_t)(pages + (npage - nbase) - base));
    for (struct Page *p = base; p < base + n; p++) {
        assert(PageReserved(p));
        p->flags = BUDDY_MANAGED;
        p->property = 0;
        set_page_ref(p, 0);
    }
    nr_free += n;
    while (n != 0) {
        unsigned order = BUDDY_MAX_ORDER;
        while (order_size(order) > n ||
               (page2ppn(base) & (order_size(order) - 1)) != 0) {
            order--;
        }
        release_block(base, order);
        base += order_size(order);
        n -= order_size(order);
    }
}

static struct Page *
buddy_alloc_pages(size_t n) {
    if (n == 0 || n > order_size(BUDDY_MAX_ORDER) || n > nr_free) {
        return NULL;
    }
    unsigned order = request_order(n);
    unsigned found = order;
    while (found <= BUDDY_MAX_ORDER && list_empty(&free_lists[found])) {
        found++;
    }
    if (found > BUDDY_MAX_ORDER) {
        return NULL;
    }
    struct Page *page = le2page(list_next(&free_lists[found]), page_link);
    list_del(&page->page_link);
    ClearPageProperty(page);
    while (found > order) {
        found--;
        add_block(page + order_size(found), found);
    }
    page->flags |= BUDDY_ALLOCATED | ((uint64_t)order << BUDDY_ORDER_SHIFT);
    page->property = n;
    nr_free -= order_size(order);
    return page;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0 && n <= order_size(BUDDY_MAX_ORDER));
    assert(base >= pages && base < pages + (npage - nbase));
    assert(base->flags & BUDDY_ALLOCATED);
    assert(base->property == n);
    unsigned order = (base->flags & BUDDY_ORDER_MASK) >> BUDDY_ORDER_SHIFT;
    assert(order == request_order(n));
    size_t count = order_size(order);
    assert(count <= (size_t)(pages + (npage - nbase) - base));
    assert((page2ppn(base) & (count - 1)) == 0);
    for (struct Page *p = base; p < base + count; p++) {
        assert((p->flags & BUDDY_MANAGED) && !PageReserved(p));
        assert(!PageProperty(p));
        p->flags = BUDDY_MANAGED;
        p->property = 0;
        set_page_ref(p, 0);
    }
    nr_free += count;
    release_block(base, order);
}

static size_t
buddy_nr_free_pages(void) {
    return nr_free;
}

static void
check_lists(void) {
    size_t total = 0;
    for (unsigned order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_entry_t *head = &free_lists[order];
        for (list_entry_t *le = list_next(head); le != head; le = list_next(le)) {
            struct Page *p = le2page(le, page_link);
            assert(PageProperty(p) && !PageReserved(p));
            assert(p->flags & BUDDY_MANAGED);
            assert(p->property == order);
            assert((page2ppn(p) & (order_size(order) - 1)) == 0);
            assert(order == BUDDY_MAX_ORDER || find_buddy(p, order) == NULL);
            total += order_size(order);
        }
    }
    assert(total == nr_free);
}

static void
buddy_check(void) {
    check_lists();
    size_t before = nr_free;
    struct Page *a = buddy_alloc_pages(1);
    struct Page *b = buddy_alloc_pages(3);
    struct Page *c = buddy_alloc_pages(17);
    assert(a != NULL && b != NULL && c != NULL);
    assert(a != b && a != c && b != c);
    assert(nr_free == before - 1 - 4 - 32);
    buddy_free_pages(b, 3);
    buddy_free_pages(a, 1);
    buddy_free_pages(c, 17);
    assert(nr_free == before);
    assert(buddy_alloc_pages(0) == NULL);
    assert(buddy_alloc_pages((size_t)-1) == NULL);
    assert(buddy_alloc_pages(order_size(BUDDY_MAX_ORDER) + 1) == NULL);
    check_lists();
    cprintf("buddy_check() succeeded!\n");
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
