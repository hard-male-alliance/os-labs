> 来源：[处理缺页异常](http://8.135.34.58/lab2026/_book/lab9/lab9_3_2_page_fault_handler.html)

<span id="处理缺页异常（page-fault异常）"></span>

### 处理缺页异常（page fault异常）

什么是缺页异常？

简单来说，当CPU试图访问一个虚拟地址，但MMU（内存管理单元）无法找到对应的物理地址映射关系，或者当前访问权限与页表项的权限设置不一致时，CPU就会触发一个缺页异常。这就像你有一个巨大的图书馆借阅卡（虚拟地址空间），但图书馆的实际藏书（物理内存）有限，当你试图借阅一本不在书架上的书时，图书馆管理员（操作系统）就需要去仓库（磁盘）取书，或者协调其他读者归还书籍。

在之前的内存管理实验中，我们主要关注的是物理内存的分配和回收，像是在管理一个真实的物理图书馆。然而，程序运行时所感知的是虚拟内存空间，它们并不关心物理内存的具体布局。这种虚拟内存的"需求"与物理内存的"供给"之间并没有直接的对应关系，需要操作系统作为中介来进行协调。在ucore中，我们通过page fault异常处理来搭建这座桥梁。

> **须知：哪些页面可以被换出？**
>
> 在操作系统的设计中，一个基本的原则是：并非所有的物理页都可以交换出去的，只有映射到用户空间且被用户程序直接访问的页面才能被交换，而被内核直接使用的内核空间的页面不能被换出。这里面的原因是什么呢？操作系统是执行的关键代码，需要保证运行的高效性和实时性，如果在操作系统执行过程中，发生了缺页现象，则操作系统不得不等很长时间（硬盘的访问速度比内存的访问速度慢 2~3 个数量级），这将导致整个系统运行低效。而且，不难想象，处理缺页过程所用到的内核代码或者数据如果被换出，整个内核都面临崩溃的危险。
>
> 不过我们当前实现的换入换出机制中，仅仅通过执行 `check_swap` 函数在内核中分配一些页，模拟对这些页的访问，然后通过 `do_pgfault` 来调用 `swap_map_swappable` 函数来查询这些页的访问情况并间接调用相关函数，换出“不常用”的页到磁盘上。

当我们引入了虚拟内存，就意味着虚拟内存的空间可以远远大于物理内存，也意味着程序可以访问"不对应物理内存页帧的虚拟内存地址"，这时CPU应当抛出 Page Fault 异常。

现在，我们需要真正实现 Page Fault 的处理机制，让操作系统能够在页面缺失时动态地分配物理内存，甚至从磁盘中换入需要的页面。这样，程序就能够透明地使用超过物理内存大小的地址空间，而无需担心内存不足的问题。

``` text
// kern/trap/trap.c

static int pgfault_handler(struct trapframe *tf) {
    extern struct mm_struct *check_mm_struct;
    if (check_mm_struct != NULL) {
        print_pgfault(tf);
    }

    struct mm_struct *mm;
    if (check_mm_struct != NULL) {
        // 测试环境：使用check_mm_struct
        assert(current == idleproc);
        mm = check_mm_struct;
    } else {
        // 真实环境：使用当前进程的内存管理结构
        if (current == NULL) {
            print_trapframe(tf);
            print_pgfault(tf);
            panic("unhandled page fault.\n");
        }
        mm = current->mm;
    }

    return do_pgfault(mm, tf->cause, tf->tval);
}
```

观察这段代码，我们可以看到缺页异常处理函数的精妙设计。它首先区分了两种不同的运行环境：在测试环境中，我们使用全局的`check_mm_struct`来模拟内存管理结构；而在真实进程环境中，则使用当前进程的`mm`结构。这种设计使得同一个处理逻辑既能用于内核自检，又能服务于真实的用户进程。

当异常发生时，`exception_handler`会将控制流转到`pgfault_handler`，进而调用`do_pgfault`函数。这个函数是内存管理的核心枢纽，它需要根据缺页的原因和地址，执行相应的处理逻辑：可能是分配一个新的物理页，也可能是从交换分区中换入一个已被换出的页。

``` text
// kern/trap/trap.c

void exception_handler(struct trapframe *tf) {
    int ret;
    switch (tf->cause) {
        /* ... 其他异常处理 ... */
        case CAUSE_LOAD_PAGE_FAULT:
            cprintf("Load page fault\n");
            if ((ret = pgfault_handler(tf)) != 0) {
                print_trapframe(tf);
                panic("handle pgfault failed. %e\n", ret);
            }
            break;
        case CAUSE_STORE_PAGE_FAULT:
            cprintf("Store/AMO page fault\n");
            if ((ret = pgfault_handler(tf)) != 0) {
                print_trapframe(tf);
                panic("handle pgfault failed. %e\n", ret);
            }
            break;
        default:
            print_trapframe(tf);
            break;
    }
}
```

处理缺页异常不仅需要分配内存，还需要在虚拟地址和物理地址之间进行转换。内核提供了一系列宏和函数来完成这项工作，它们像是操作系统中的"翻译官"，确保虚拟世界和物理世界能够正确对应。这些转换函数是内核基础设施的重要组成部分，使得上层代码能够透明地访问物理内存。

``` text
// kern/mm/pmm.h

// 内核虚拟地址到物理地址的转换
#define PADDR(kva) ({                                   \
    uintptr_t __m_kva = (uintptr_t)(kva);               \
    if (__m_kva < KERNBASE) {                           \
        panic("PADDR called with invalid kva %08lx", __m_kva); \
    }                                                   \
    __m_kva - va_pa_offset;                             \
})

// 物理地址到内核虚拟地址的转换
#define KADDR(pa) ({                                    \
    uintptr_t __m_pa = (pa);                            \
    size_t __m_ppn = PPN(__m_pa);                       \
    if (__m_ppn >= npage) {                             \
        panic("KADDR called with invalid pa %08lx", __m_pa); \
    }                                                   \
    (void *)(__m_pa + va_pa_offset);                    \
})

// Page结构体到内核虚拟地址的转换
static inline void *page2kva(struct Page *page) {
    return KADDR(page2pa(page));
}
```

当`do_pgfault`函数被调用时，它面临的核心挑战是：如何处理那些不在物理内存中的页面？这就是页面置换算法发挥作用的地方。如果缺页的地址对应着一个已被换出到磁盘的页面，那么`do_pgfault`需要协调`swap`子系统，从磁盘中读回页面内容，并为其分配物理内存。同时，它还需要更新页表，建立虚拟地址到新物理页面的映射关系。

``` text
// kern/mm/vmm.c
int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr) {
    int ret = -E_INVAL;
    // 1. 查找 vma 结构与地址合法性检查
    struct vma_struct *vma = find_vma(mm, addr);
    pgfault_num++;
    if (vma == NULL || vma->vm_start > addr) {
        return -E_INVAL;
    }

    // 2. 写权限检查（仅对写操作检查）
    // RISC-V: cause == 15 (CAUSE_STORE_PAGE_FAULT) 表示写操作
    bool is_write = (error_code == 15);
    if (is_write && !(vma->vm_flags & VM_WRITE)) {
        return -E_INVAL;
    }

    // 3. 设置页权限：RISC-V 要求必须设置 PTE_R
    uint32_t perm = PTE_U | PTE_R;
    if (vma->vm_flags & VM_WRITE) {
        perm |= PTE_W;
    }

    addr = ROUNDDOWN(addr, PGSIZE);
    // 4. 获取 PTE（必要时创建页表）
    pte_t *ptep = get_pte(mm->pgdir, addr, 1);
    if (ptep == NULL) {
        return -E_NO_MEM;
    }

    /*
     * 5. mmap 文件映射缺页处理
     * 如果 vma->vm_file 不为空，说明是文件映射
     */
    if (vma->vm_file != NULL) {
    }

    /*
     * 6. 匿名页 / swap 缺页处理
     */
    if (*ptep == 0) {
        // 匿名页：分配新页并清零
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) {
            return -E_NO_MEM;
        }
    } else {
        // PTE不为0，可能是swap条目
        if (swap_init_ok) {
            struct Page *page = NULL;
            // 从 swap 分区换入页面
            if ((ret = swap_in(mm, addr, &page)) != 0) {
                return ret;
            }
            // 建立映射并更新可交换标记
            page_insert(mm->pgdir, page, addr, perm);
            swap_map_swappable(mm, addr, page, 1);
            page->pra_vaddr = addr;
        }
        // ... 其他情况（如 swap 未初始化时的错误处理） ...
    }

    return 0;
}
```

缺页异常处理机制识别了页面不在内存中的情况。但这自然地引出了一个新的问题：如果物理内存已满，没有空闲页面容纳新换入的页面，该怎么办？这就需要页面置换算法来做出选择，决定哪些页面应该被换出到磁盘。接下来，我们就去看看页面置换算法是如何设计和实现的。
