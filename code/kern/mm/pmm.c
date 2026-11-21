#include <default_pmm.h>
#include <defs.h>
#include <error.h>
#include <kmalloc.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <dtb.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>
#include <vmm.h>
#include <riscv.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// The kernel image is mapped at VA=KERNBASE and PA=info.base
uint_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
pde_t *boot_pgdir_va = NULL;
// physical address of boot-time page directory
uintptr_t boot_pgdir_pa;

// physical memory management
const struct pmm_manager *pmm_manager;

static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void)
{
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s (best fit)\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n)
{
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n)
{
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void)
{
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

//lab2 function1 2413077 2410824 2413174
static void page_init(void) {
    extern char end[];
    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();

    const uintptr_t kernel_start = KERNBASE - PHYSICAL_MEMORY_OFFSET;
    const uintptr_t mapped_end = KERNTOP - PHYSICAL_MEMORY_OFFSET;
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
    if (mem_base == 0 || mem_size == 0 || mem_size > (uint64_t)-1 - mem_base) {
        panic("invalid physical memory range from DTB");
    }
    uint64_t mem_end = mem_base + mem_size;
    if (mem_base > kernel_start || mem_end <= kernel_start) {
        panic("DTB memory does not contain the kernel");
    }
    cprintf("physical memory map:\n");
    cprintf("  memory: [0x%016lx, 0x%016lx)\n", mem_base, mem_end);

    // The boot mapping is fixed; DTB describes RAM, not our mapping capacity.
    uintptr_t managed_end = ROUNDDOWN(mem_end < mapped_end ? mem_end : mapped_end, PGSIZE);
    uintptr_t kernel_end = PADDR(end);
    if (kernel_end < kernel_start || kernel_end > managed_end) {
        panic("kernel lies outside manageable physical memory");
    }
    uintptr_t descriptors_pa = ROUNDUP(kernel_end, PGSIZE);
    npage = PPN(managed_end);
    if (npage <= nbase) {
        panic("no manageable physical pages");
    }
    size_t descriptors_size = (npage - nbase) * sizeof(struct Page);
    if (descriptors_pa > managed_end || descriptors_size > managed_end - descriptors_pa) {
        panic("physical memory cannot hold page descriptors");
    }
    pages = (struct Page *)(descriptors_pa + va_pa_offset);
    for (size_t i = 0; i < npage - nbase; i++) {
        pages[i].flags = 0;
        pages[i].property = 0;
        set_page_ref(&pages[i], 0);
        list_init(&pages[i].page_link);
        SetPageReserved(&pages[i]);
    }

    uintptr_t free_start = ROUNDUP(descriptors_pa + descriptors_size, PGSIZE);
    cprintf("  managed: [0x%016lx, 0x%016lx), free starts at 0x%016lx\n",
            (uintptr_t)DRAM_BASE, managed_end, free_start);
    if (free_start < managed_end) {
        init_memmap(pa2page(free_start), (managed_end - free_start) / PGSIZE);
    }
}

// pmm_init - setup a pmm to manage physical memory, build PDT&PT to setup
// paging mechanism
//         - check the correctness of pmm & paging mechanism, print PDT&PT
void pmm_init(void)
{
    // We need to alloc/free the physical memory (granularity is 4KB or other
    // size).
    // So a framework of physical memory manager (struct pmm_manager)is defined
    // in pmm.h
    // First we should init a physical memory manager(pmm) based on the
    // framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a
    // pmm
    check_alloc_page();

    // create boot_pgdir, an initial page directory(Page Directory Table, PDT)
    extern char boot_page_table_sv39[];
    boot_pgdir_va = (pte_t *)boot_page_table_sv39;
    boot_pgdir_pa = PADDR(boot_pgdir_va);

    check_pgdir();

    static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0);

    // now the basic virtual memory map(see memalyout.h) is established.
    // check the correctness of the basic virtual memory map.
    check_boot_pgdir();

    kmalloc_init();
}

// get_pte - get pte and return the kernel virtual address of this pte for la
//        - if the PT contians this pte didn't exist, alloc a page for PT
// parameter:
//  pgdir:  the kernel virtual base address of PDT
//  la:     the linear address need to map
//  create: a logical value to decide if alloc a page for PT
// return vaule: the kernel virtual address of this pte
static struct Page *alloc_pagetable(void)
{
    struct Page *page = alloc_page();
    if (page != NULL)
    {
        set_page_ref(page, 1);
        memset(page2kva(page), 0, PGSIZE);
    }
    return page;
}

static bool pgdir_active(pde_t *pgdir)
{
    return read_csr(satp) == (((uintptr_t)SATP_MODE_SV39 << 60) |
                              (PADDR(pgdir) >> PGSHIFT));
}

// Find a 4 KiB slot without descending through the boot superpage.
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    pde_t *root = &pgdir[PDX1(la)];
    struct Page *new_root_child = NULL;
    pde_t *middle;
    if (*root & PTE_V)
    {
        if (*root & (PTE_R | PTE_W | PTE_X))
            return NULL;
        middle = KADDR(PDE_ADDR(*root));
    }
    else
    {
        if (!create || (new_root_child = alloc_pagetable()) == NULL)
            return NULL;
        middle = page2kva(new_root_child);
    }

    pde_t *entry = &middle[PDX0(la)];
    if (!(*entry & PTE_V))
    {
        struct Page *leaf = create ? alloc_pagetable() : NULL;
        if (leaf == NULL)
        {
            if (new_root_child != NULL)
                free_page(new_root_child);
            return NULL;
        }
        *entry = ptd_create(page2ppn(leaf));
        if (new_root_child != NULL)
            *root = ptd_create(page2ppn(new_root_child));
        if (pgdir_active(pgdir))
            flush_tlb();
    }
    else if (*entry & (PTE_R | PTE_W | PTE_X))
    {
        return NULL;
    }

    return &((pte_t *)KADDR(PDE_ADDR(*entry)))[PTX(la)];
}

// get_page - get related Page struct for linear address la using PDT pgdir
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep_store != NULL)
    {
        *ptep_store = ptep;
    }
    if (ptep != NULL && *ptep & PTE_V)
    {
        return pte2page(*ptep);
    }
    return NULL;
}

//LAB4 function1 2413077 2410824 2413174
static void put_page(struct Page *page)
{
    if (page_ref_dec(page) == 0)
        free_page(page);
}

int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
{
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL)
        return -E_NO_MEM;

    struct Page *old = (*ptep & PTE_V) ? pte2page(*ptep) : NULL;
    if (old != page)
        page_ref_inc(page);

    *ptep = pte_create(page2ppn(page), perm);
    tlb_invalidate(pgdir, la);
    if (old != NULL && old != page)
        put_page(old);
    return 0;
}

void page_remove(pde_t *pgdir, uintptr_t la)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep == NULL || !(*ptep & PTE_V))
        return;

    struct Page *page = pte2page(*ptep);
    *ptep = 0;
    tlb_invalidate(pgdir, la);
    put_page(page);
}

void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{
    if (pgdir_active(pgdir))
        asm volatile("sfence.vma %0, zero" : : "r"(la) : "memory");
}

// pgdir_alloc_page - call alloc_page & page_insert functions to
//                  - allocate a page size memory & setup an addr map
//                  - pa<->la with linear address la and the PDT pgdir
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm)
{
    struct Page *page = alloc_page();
    if (page != NULL)
    {
        if (page_insert(pgdir, page, la, perm) != 0)
        {
            free_page(page);
            return NULL;
        }
        // swap_map_swappable(check_mm_struct, la, page, 0);
        page->pra_vaddr = la;
        assert(page_ref(page) == 1);
        // cprintf("get No. %d  page: pra_vaddr %x, pra_link.prev %x,
        // pra_link_next %x in pgdir_alloc_page\n", (page-pages),
        // page->pra_vaddr,page->pra_page_link.prev,
        // page->pra_page_link.next);
    }

    return page;
}

static void check_alloc_page(void)
{
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

static void check_pgdir(void)
{
    // assert(npage <= KMEMSIZE / PGSIZE);
    // The memory starts at 2GB in RISC-V
    // so npage is always larger than KMEMSIZE / PGSIZE
    size_t nr_free_store;

    nr_free_store = nr_free_pages();

    assert(npage <= KERNTOP / PGSIZE);
    assert(boot_pgdir_va != NULL && (uint32_t)PGOFF(boot_pgdir_va) == 0);
    assert(get_page(boot_pgdir_va, 0x0, NULL) == NULL);

    struct Page *p1, *p2;
    p1 = alloc_page();
    assert(page_insert(boot_pgdir_va, p1, 0x0, 0) == 0);

    pte_t *ptep;
    assert((ptep = get_pte(boot_pgdir_va, 0x0, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert(page_ref(p1) == 1);

    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir_va[0]));
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
    assert(get_pte(boot_pgdir_va, PGSIZE, 0) == ptep);

    p2 = alloc_page();
    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(*ptep & PTE_U);
    assert(*ptep & PTE_W);
    assert((boot_pgdir_va[0] & (PTE_U | PTE_A | PTE_D)) == 0);
    assert(page_ref(p2) == 1);

    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2);
    assert(page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert((*ptep & PTE_U) == 0);

    page_remove(boot_pgdir_va, 0x0);
    assert(page_ref(p1) == 1);
    assert(page_ref(p2) == 0);

    page_remove(boot_pgdir_va, PGSIZE);
    assert(page_ref(p1) == 0);
    assert(page_ref(p2) == 0);

    assert(page_ref(pde2page(boot_pgdir_va[0])) == 1);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_pgdir() succeeded!\n");
}

static void check_boot_pgdir(void)
{
    size_t nr_free_store;
    nr_free_store = nr_free_pages();

    // entry.S maps the entire managed range through one Sv39 level-2 leaf.
    pte_t direct = boot_pgdir_va[PDX1(KERNBASE)];
    assert((direct & (PTE_V | PTE_R | PTE_W | PTE_X)) ==
           (PTE_V | PTE_R | PTE_W | PTE_X));
    assert(PTE_ADDR(direct) == DRAM_BASE);
    assert(PDX1((uintptr_t)KADDR(npage * PGSIZE - 1)) == PDX1(KERNBASE));

    assert(boot_pgdir_va[0] == 0);

    struct Page *p;
    p = alloc_page();
    assert(page_insert(boot_pgdir_va, p, 0x100, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 1);
    assert(page_insert(boot_pgdir_va, p, 0x100 + PGSIZE, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 2);

    const char *str = "ucore: Hello world!!";
    strcpy((void *)0x100, str);
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

    *(char *)(page2kva(p) + 0x100) = '\0';
    assert(strlen((const char *)0x100) == 0);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(p);
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_boot_pgdir() succeeded!\n");
}

//lab5 function3 2413077 2410824 2413174
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share)
{
    assert(PGOFF(start) == 0 && PGOFF(end) == 0 && USER_ACCESS(start, end));

    while (start < end)
    {
        pte_t *src = get_pte(from, start, false);
        if (src == NULL)
        {
            start = ROUNDDOWN(start, PTSIZE) + PTSIZE;
            continue;
        }
        if (*src & PTE_V)
        {
            uint32_t perm = *src & (PTE_USER | PTE_COW);
            if (!share && (perm & PTE_W))
                perm = (perm & ~PTE_W) | PTE_COW;
            int ret = page_insert(to, pte2page(*src), start, perm);
            if (ret != 0)
                return ret;
            if ((*src & PTE_W) && (perm & PTE_COW))
                *src = (*src & ~PTE_W) | PTE_COW;
        }
        start += PGSIZE;
    }
    return 0;
}

int resolve_cow(pde_t *pgdir, uintptr_t addr)
{
    uintptr_t va = ROUNDDOWN(addr, PGSIZE);
    if (!USER_ACCESS(va, va + PGSIZE))
        return -E_FAULT;
    pte_t *pte = get_pte(pgdir, va, false);
    if (pte == NULL || (*pte & (PTE_V | PTE_U | PTE_COW)) != (PTE_V | PTE_U | PTE_COW))
        return -E_FAULT;

    struct Page *page = pte2page(*pte);
    uint32_t perm = (*pte & PTE_USER) | PTE_W;
    if (page_ref(page) == 1)
        return page_insert(pgdir, page, va, perm);

    struct Page *copy = alloc_page();
    if (copy == NULL)
        return -E_NO_MEM;
    memcpy(page2kva(copy), page2kva(page), PGSIZE);
    int ret = page_insert(pgdir, copy, va, perm);
    if (ret != 0)
        free_page(copy);
    return ret;
}

static void free_user_table(pde_t *table, unsigned level, unsigned entries)
{
    for (unsigned i = 0; i < entries; i++)
    {
        pte_t entry = table[i];
        if (!(entry & PTE_V))
            continue;
        table[i] = 0;
        struct Page *page = pte2page(entry);
        if (level != 0)
            free_user_table(page2kva(page), level - 1, NPTEENTRY);
        put_page(page);
    }
}

void free_user_pgdir(pde_t *pgdir)
{
    // The caller has detached the address space; high-half entries are borrowed.
    free_user_table(pgdir, 2, USERTOP / PDSIZE);
}
