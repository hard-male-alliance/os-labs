// Include the production implementation so list invariants are checked too.
#include "../code/kern/mm/best_fit_pmm.c"
static struct Page arena[4096];
struct Page *pages = arena;
size_t npage = DRAM_BASE / PGSIZE + 4096;
const size_t nbase = DRAM_BASE / PGSIZE;
uint64_t va_pa_offset;
const struct pmm_manager *pmm_manager = &best_fit_pmm_manager;
struct Page *alloc_pages(size_t n) { return pmm_manager->alloc_pages(n); }
void free_pages(struct Page *p, size_t n) { pmm_manager->free_pages(p, n); }
size_t nr_free_pages(void) { return pmm_manager->nr_free_pages(); }
int cprintf(const char *fmt, ...) { return 0; }
void __panic(const char *file, int line, const char *fmt, ...) {
    __builtin_printf("panic at %s:%d: %s\n", file, line, fmt);
    __builtin_trap();
}
static unsigned long state = 88435;
static unsigned long random_next(void) { state = state * 1664525 + 1013904223; return state >> 16; }
static unsigned char used[4096];
struct allocation { struct Page *page; size_t size; } live[256];
static void verify(void) {
    size_t free_count = 0;
    struct Page *end = NULL;
    unsigned char seen[4096] = {0};
    for (list_entry_t *le = list_next(&free_list); le != &free_list; le = list_next(le)) {
        struct Page *p = le2page(le, page_link);
        assert(end == NULL || end < p);
        assert(PageProperty(p) && p->property > 0);
        end = p + p->property;
        assert(end <= arena + 4096);
        for (size_t i = 0; i < p->property; i++) {
            size_t index = p + i - arena;
            assert(!used[index] && !seen[index]);
            seen[index] = 1;
            free_count++;
        }
    }
    for (int i = 0; i < 4096; i++) { assert(seen[i] == !used[i]); }
    assert(free_count == nr_free_pages());
}
int main(void) {
    best_fit_init();
    for (int i = 0; i < 4096; i++) { SetPageReserved(arena + i); }
    best_fit_init_memmap(arena, 4096);
    best_fit_check();
    verify();
    for (int step = 0; step < 50000; step++) {
        int slot = random_next() % 256;
        if (live[slot].page != NULL) {
            struct Page *p = live[slot].page;
            for (size_t i = 0; i < live[slot].size; i++) { used[p - arena + i] = 0; }
            free_pages(p, live[slot].size);
            live[slot].page = NULL;
        } else {
            size_t n = random_next() % 32 + 1;
            size_t best_size = 4097, best_index = 0;
            for (size_t i = 0; i < 4096;) {
                if (used[i]) { i++; continue; }
                size_t begin = i;
                while (i < 4096 && !used[i]) { i++; }
                size_t length = i - begin;
                if (length >= n && length < best_size) { best_size = length; best_index = begin; }
            }
            struct Page *p = alloc_pages(n);
            assert((p == NULL) == (best_size == 4097));
            if (p != NULL) {
                assert(p == arena + best_index);
                for (size_t i = 0; i < n; i++) { assert(!used[p - arena + i]); used[p - arena + i] = 1; }
                live[slot] = (struct allocation){p, n};
            }
        }
        verify();
    }
    for (int slot = 0; slot < 256; slot++) {
        if (live[slot].page != NULL) {
            struct Page *p = live[slot].page;
            for (size_t i = 0; i < live[slot].size; i++) { used[p - arena + i] = 0; }
            free_pages(p, live[slot].size);
        }
    }
    verify();
    assert(nr_free_pages() == 4096);
    __builtin_printf("Best-Fit: framework checks and 50000 model-checked operations PASS\n");
}
