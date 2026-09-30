> 来源：[页替换算法和机制实现](http://8.135.34.58/lab2026/_book/lab9/lab9_3_3_page_swapping.html)

<span id="页替换算法和机制实现"></span>

### 页替换算法和机制实现

<span id="页替换算法设计思路"></span>

#### 页替换算法设计思路

操作系统为何要进行页面置换呢？这是由于操作系统给用户态的应用程序提供了一个虚拟的“大容量”内存空间，而实际的物理内存空间又没有那么大。所以操作系统就就“瞒着”应用程序，只把应用程序中“常用”的数据和代码放在物理内存中，而不常用的数据和代码放在了硬盘这样的存储介质上。如果应用程序访问的是“常用”的数据和代码，那么操作系统已经放置在内存中了，不会出现什么问题。但当应用程序访问它认为应该在内存中的的数据或代码时，如果这些数据或代码不在内存中，则根据上一小节的介绍，会产生页访问异常。这时，操作系统必须能够应对这种页访问异常，即尽快把应用程序当前需要的数据或代码放到内存中来，然后重新执行应用程序产生异常的访存指令。如果在把硬盘中对应的数据或代码调入内存前，操作系统发现物理内存已经没有空闲空间了，这时操作系统必须把它认为“不常用”的页换出到磁盘上去，以腾出内存空闲空间给应用程序所需的数据或代码。

操作系统迟早会碰到没有内存空闲空间而必须要置换出内存中某个“不常用”的页的情况。如何判断内存中哪些是“常用”的页，哪些是“不常用”的页，把“常用”的页保持在内存中，在物理内存空闲空间不够的情况下，把“不常用”的页置换到硬盘上就是页替换算法着重考虑的问题。容易理解，一个好的页替换算法会导致页访问异常次数少，也就意味着访问硬盘的次数也少，从而使得应用程序执行的效率就高。

本次实验涉及的页替换算法（包括扩展练习）：

- 先进先出(First In First Out, FIFO)页替换算法：该算法总是淘汰最先进入内存的页，即选择在内存中驻留时间最久的页予以淘汰。只需把一个应用程序在执行过程中已调入内存的页按先后次序链接成一个队列，队列头指向内存中驻留时间最久的页，队列尾指向最近被调入内存的页。这样需要淘汰页时，从队列头很容易查找到需要淘汰的页。FIFO 算法只是在应用程序按线性顺序访问地址空间时效果才好，否则效率不高。因为那些常被访问的页，往往在内存中也停留得最久，结果它们因变“老”而不得不被置换出去。FIFO 算法的另一个缺点是，它有一种异常现象（Belady 现象），即在增加放置页的物理页帧的情况下，反而使页访问异常次数增多。
- 最久未使用(least recently used, LRU)算法：利用局部性，通过过去的访问情况预测未来的访问情况，我们可以认为最近还被访问过的页面将来被访问的可能性大，而很久没访问过的页面将来不太可能被访问。于是我们比较当前内存里的页面最近一次被访问的时间，把上一次访问时间离现在最久的页面置换出去。
- 时钟（Clock）页替换算法：是 LRU 算法的一种近似实现。时钟页替换算法把各个页面组织成环形链表的形式，类似于一个钟的表面。然后把一个指针（简称当前指针）指向最老的那个页面，即最先进来的那个页面。另外，时钟算法需要在页表项（PTE）中设置了一位访问位来表示此页表项对应的页当前是否被访问过。当该页被访问时，CPU 中的 MMU 硬件将把访问位置“1”。当操作系统需要淘汰页时，对当前指针指向的页所对应的页表项进行查询，如果访问位为“0”，则淘汰该页，如果该页被写过，则还要把它换出到硬盘上；如果访问位为“1”，则将该页表项的此位置“0”，继续访问下一个页。该算法近似地体现了 LRU 的思想，且易于实现，开销少，需要硬件支持来设置访问位。时钟页替换算法在本质上与 FIFO 算法是类似的，不同之处是在时钟页替换算法中跳过了访问位为 1 的页。
- 改进的时钟（Enhanced Clock）页替换算法：在时钟置换算法中，淘汰一个页面时只考虑了页面是否被访问过，但在实际情况中，还应考虑被淘汰的页面是否被修改过。因为淘汰修改过的页面还需要写回硬盘，使得其置换代价大于未修改过的页面，所以优先淘汰没有修改的页，减少磁盘操作次数。改进的时钟置换算法除了考虑页面的访问情况，还需考虑页面的修改情况。即该算法不但希望淘汰的页面是最近未使用的页，而且还希望被淘汰的页是在主存驻留期间其页面内容未被修改过的。这需要为每一页的对应页表项内容中增加一位引用位和一位修改位。当该页被访问时，CPU 中的 MMU 硬件将把访问位置“1”。当该页被“写”时，CPU 中的 MMU 硬件将把修改位置“1”。这样这两位就存在四种可能的组合情况：（0，0）表示最近未被引用也未被修改，首先选择此页淘汰；（0，1）最近未被使用，但被修改，其次选择；（1，0）最近使用而未修改，再次选择；（1，1）最近使用且修改，最后选择。该算法与时钟算法相比，可进一步减少磁盘的 I/O 操作次数，但为了查找到一个尽可能适合淘汰的页面，可能需要经过多次扫描，增加了算法本身的执行开销。

<span id="页面置换机制设计思路"></span>

#### 页面置换机制设计思路

现在我们来看看ucore页面置换机制的实现。

页面置换机制中， 我们需要维护一些”不在内存当中但是也许会用到“的页，它们存储在磁盘的交换区里，也有对应的虚拟地址，但是因为它们不在内存里，在页表里并没有对它们的虚拟地址进行映射。但是在发生Page Fault之后，会把访问到的页放到内存里，这时也许会把其他页扔出去，来给要用的页腾出地方。页面置换算法的核心任务，主要就是确定”把谁扔出去“。

页表里的信息大家都知道内容方面是比较有限的，基本上可以理解为"当前哪些数据在内存条里以及它们物理地址和虚拟地址的对应关系", 这里我们显然需要一些页表之外的数据结构来维护当前页表里没映射的页。也就是要存储以下这些信息：

- 有哪些虚拟地址对应的页当前在磁盘上，分别在磁盘上的哪个位置？
- 有哪些虚拟地址对应的页面当前放在内存里？

这两类页面（位于内存/磁盘）因为会相互转换(换入/换出内存)，所以我们将这两类页面一起维护，也就是维护”所有可用的虚拟地址/虚拟页的集合“（不论当前这个虚拟地址对应的页在内存上还是在硬盘上）。之后我们将要实现进程机制，对于不同的进程，可用的虚拟地址（虚拟页）的集合常常是不一样的，因此每个进程需要一个页表，也需要一个数据结构来维护“所有可用的虚拟地址”。

因此，我们在vmm.h定义两个结构体 (vmm：virtural memory management)。

- `vma_struct`结构体描述一段连续的虚拟地址，从`vm_start`到`vm_end`。 通过包含一个`list_entry_t`成员，我们可以把同一个页表对应的多个`vma_struct`结构体串成一个链表，在链表里把它们按照区间的起始点进行排序。

- `vm_flags`表示的是一段虚拟地址对应的权限（可读，可写，可执行等），这个权限在页表项里也要进行对应的设置。

我们注意到，每个页表（每个虚拟地址空间）可能包含多个`vma_struct`, 也就是多个访问权限可能不同的，不相交的连续地址区间。我们用`mm_struct`结构体把一个页表对应的信息组合起来，包括`vma_struct`链表的首指针，对应的页表在内存里的指针，`vma_struct`链表的元素个数。

（参考`libs/list.h`）

``` text
// kern/mm/vmm.h
//pre define
struct mm_struct;

// the virtual continuous memory area(vma), [vm_start, vm_end),
// addr belong to a vma means  vma.vm_start<= addr <vma.vm_end
struct vma_struct {
    struct mm_struct *vm_mm; // the set of vma using the same PDT
    uintptr_t vm_start;      // start addr of vma
    uintptr_t vm_end;        // end addr of vma, not include the vm_end itself
    uint32_t vm_flags;       // flags of vma
    list_entry_t list_link;  // linear list link which sorted by start addr of vma
    int vm_pgoff;            // file offset in pages
    struct file *vm_file;    // mapped file pointer
};

#define le2vma(le, member)                  \
    to_struct((le), struct vma_struct, member)

#define VM_READ                 0x00000001
#define VM_WRITE                0x00000002
#define VM_EXEC                 0x00000004
#define VM_STACK                0x00000008

// the control struct for a set of vma using the same Page Table
struct mm_struct {
    list_entry_t mmap_list;        // linear list link which sorted by start addr of vma
    struct vma_struct *mmap_cache; // current accessed vma, used for speed purpose
    pde_t *pgdir;                  // the Page Table of these vma
    int map_count;                 // the count of these vma
    void *sm_priv;                   // the private data for swap manager
    int mm_count;                  // the number ofprocess which shared the mm
    semaphore_t mm_sem;            // mutex for using dup_mmap fun to duplicat the mm
    int locked_by;
};
```

除了以上内容，我们还需要为`vma_struct`和`mm_struct`定义和实现一些接口：包括它们的构造函数，以及如何把新的`vma_struct`插入到`mm_struct`对应的链表里。注意这两个结构体占用的内存空间需要用`kmalloc()`函数动态分配。

``` text
// kern/mm/vmm.c
// mm_create -  alloc a mm_struct & initialize it.
struct mm_struct *
mm_create(void) {
    struct mm_struct *mm = kmalloc(sizeof(struct mm_struct));

    if (mm != NULL) {
        list_init(&(mm->mmap_list));
        mm->mmap_cache = NULL;
        mm->pgdir = NULL;
        mm->map_count = 0;

        if (swap_init_ok) swap_init_mm(mm);//我们接下来解释页面置换的初始化
        else mm->sm_priv = NULL;

        set_mm_count(mm, 0);
        sem_init(&(mm->mm_sem), 1);
    }
    return mm;
}

// vma_create - alloc a vma_struct & initialize it. (addr range: vm_start~vm_end)
struct vma_struct *
vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags) {
    struct vma_struct *vma = kmalloc(sizeof(struct vma_struct));
    if (vma != NULL) {
        vma->vm_start = vm_start;
        vma->vm_end = vm_end;
        vma->vm_flags = vm_flags;
        vma->vm_file = NULL; // 初始化文件映射指针
        vma->vm_pgoff = 0;   // 初始化页偏移
    }
    return vma;
}
```

在插入一个新的`vma_struct`之前，我们要保证它和原有的区间都不重合。

``` text
// kern/mm/vmm.c
// check_vma_overlap - check if vma1 overlaps vma2 ?
static inline void
check_vma_overlap(struct vma_struct *prev, struct vma_struct *next) {
    assert(prev->vm_start < prev->vm_end);
    assert(prev->vm_end <= next->vm_start);
    assert(next->vm_start < next->vm_end);// next 是我们想插入的区间，这里顺便检验了start < end
}
```

我们可以插入一个新的`vma_struct`, 也可以查找某个虚拟地址对应的`vma_struct`是否存在。

``` text
// kern/mm/vmm.c

// insert_vma_struct -insert vma in mm's list link
void
insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma) {
    assert(vma->vm_start < vma->vm_end);
    list_entry_t *list = &(mm->mmap_list);
    list_entry_t *le_prev = list, *le_next;

    list_entry_t *le = list;
    while ((le = list_next(le)) != list) {
        struct vma_struct *mmap_prev = le2vma(le, list_link);
        if (mmap_prev->vm_start > vma->vm_start) {
            break;
        }
        le_prev = le;
    }
    //保证插入后所有vma_struct按照区间左端点有序排列
    le_next = list_next(le_prev);

    /* check overlap */
    if (le_prev != list) {
        check_vma_overlap(le2vma(le_prev, list_link), vma);
    }
    if (le_next != list) {
        check_vma_overlap(vma, le2vma(le_next, list_link));
    }

    vma->vm_mm = mm;
    list_add_after(le_prev, &(vma->list_link));

    mm->map_count ++;//计数器
}

// find_vma - find a vma  (vma->vm_start <= addr <= vma_vm_end)
//如果返回NULL，说明查询的虚拟地址不存在/不合法，既不对应内存里的某个页，也不对应硬盘里某个可以换进来的页
struct vma_struct *
find_vma(struct mm_struct *mm, uintptr_t addr) {
    struct vma_struct *vma = NULL;
    if (mm != NULL) {
        vma = mm->mmap_cache;
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr)) {
                bool found = 0;
                list_entry_t *list = &(mm->mmap_list), *le = list;
                while ((le = list_next(le)) != list) {
                    vma = le2vma(le, list_link);
                    if (vma->vm_start<=addr && addr < vma->vm_end) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    vma = NULL;
                }
        }
        if (vma != NULL) {
            mm->mmap_cache = vma;
        }
    }
    return vma;
}
```

如果此时发生Page Fault怎么办？我们可以回顾之前的异常处理部分的知识。我们的`trapFrame`传递了`tf->tval`给`do_pgfault()`函数，而这实际上是`stval`这个寄存器的数值（在旧版的RISCV标准里叫做`sbadvaddr`)，这个寄存器存储一些关于异常的数据，对于`PageFault`它存储的是访问出错的虚拟地址。

``` text
// kern/trap/trap.c
static int pgfault_handler(struct trapframe *tf) {
    extern struct mm_struct *check_mm_struct;//当前使用的mm_struct的指针，在vmm.c定义
    print_pgfault(tf);
    if (check_mm_struct != NULL) {
        return do_pgfault(check_mm_struct, tf->cause, tf->tval);
    }
    panic("unhandled page fault.\n");
}
// kern/mm/vmm.c
struct mm_struct *check_mm_struct;

// check_pgfault - check correctness of pgfault handler
static void
check_pgfault(void) {
    /* ...... */
    check_mm_struct = mm_create();
    /* ...... */
}
```

`do_pgfault()`函数在`vmm.c`定义，是页面置换机制的核心。如果过程可行，没有错误值返回，我们就可对页表做对应的修改，通过加入对应的页表项，并把硬盘上的数据换进内存，这时还可能涉及到要把内存里的一个页换出去，而`do_pgfault()`函数就实现了这些功能。如果你对这部分内容相当了解的话，那可以说你对于页面置换机制的掌握程度已经很棒了。

``` text
// kern/mm/vmm.c
int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr)
{
    int ret = -E_INVAL;
    //try to find a vma which include addr
    struct vma_struct *vma = find_vma(mm, addr);
    //我们首先要做的就是在mm_struct里判断这个虚拟地址是否可用
    pgfault_num++;

    // 1. 地址合法性检查
    if (vma == NULL || vma->vm_start > addr)
    {
        cprintf("not valid addr %x, and can not find it in vma\n", addr);
        return -E_INVAL;
    }

    // 2. 写权限检查（仅对写操作检查）
    // RISC-V: cause == 15 (CAUSE_STORE_PAGE_FAULT) 表示写操作
    bool is_write = (error_code == 15);
    if (is_write && !(vma->vm_flags & VM_WRITE))
    {
        cprintf("do_pgfault: write to non-writable vma\n");
        return -E_INVAL;
    }

    // 3. 设置页权限
    // RISC-V 要求：必须设置 PTE_R 才能访问页面
    uint32_t perm = PTE_U | PTE_R; // 用户可访问 + 可读
    if (vma->vm_flags & VM_WRITE)
    {
        perm |= PTE_W;
    }

    addr = ROUNDDOWN(addr, PGSIZE);

    // 4. 获取 PTE（必要时创建页表）
    pte_t *ptep = get_pte(mm->pgdir, addr, 1);
    if (ptep == NULL)
    {
        cprintf("get_pte in do_pgfault failed\n");
        return -E_NO_MEM;
    }

    /*
     * =================================================
     * 5. mmap 文件映射缺页处理
     *    模仿 Linux 的处理方式：
     *    - 文件映射的页面在缺页时从文件读取
     *    - 使用 file_seek + file_read 来读取指定偏移的数据
     * =================================================
     */
    if (vma->vm_file != NULL)
    {
        // 文件映射：只处理"页尚未建立"的情况
        if (*ptep == 0)
        {
            // 分配物理页
            struct Page *page = pgdir_alloc_page(mm->pgdir, addr, perm);
            if (page == NULL)
            {
                cprintf("pgdir_alloc_page in mmap pgfault failed\n");
                return -E_NO_MEM;
            }

            void *kva = page2kva(page);
            // 先清零，防止文件末尾不足一页时有脏数据
            memset(kva, 0, PGSIZE);

            // 计算文件偏移
            // vm_pgoff 是映射起始的页偏移，加上当前页相对于映射起始的偏移
            size_t page_index = (addr - vma->vm_start) / PGSIZE;
            off_t file_offset = (vma->vm_pgoff + page_index) * PGSIZE;

            // 先 seek 到正确位置，再读取
            struct file *file = vma->vm_file;
            int fd = file->fd;
            off_t saved_pos = file->pos; // 保存当前游标

            ret = file_seek(fd, file_offset, LSEEK_SET);
            if (ret != 0)
            {
                cprintf("file_seek in mmap pgfault failed: %e\n", ret);
                // 即使 seek 失败，页面已分配，返回成功让进程继续
                // 页面内容为零
                return 0;
            }

            size_t read_size = 0;
            ret = file_read(fd, kva, PGSIZE, &read_size);

            // 恢复文件游标
            file_seek(fd, saved_pos, LSEEK_SET);

            if (ret != 0 && ret != -E_EOF)
            {
                cprintf("file_read in mmap pgfault failed: %e\n", ret);
                // 读取失败，但页面已分配，返回成功
                return 0;
            }
            // 读取成功（可能读取了 0 到 PGSIZE 字节）
        }
        // 如果 *ptep != 0，说明页面已存在，直接返回成功
        return 0;
    }

    /*
     * ==================================
     * 6. 匿名页 / swap 缺页处理
     * ==================================
     */
    if (*ptep == 0)
    {
        // 匿名页：分配新页并清零
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL)
        {
            cprintf("pgdir_alloc_page in do_pgfault failed\n");
            return -E_NO_MEM;
        }
    }
    else
    {
        // PTE不为0，可能是swap条目或者是已存在的页面
        if (swap_init_ok)
        {
            // swap 页：从交换区换入
            struct Page *page = NULL;
            if ((ret = swap_in(mm, addr, &page)) != 0)
            {
                cprintf("swap_in in do_pgfault failed\n");
                return ret;
            }
            page_insert(mm->pgdir, page, addr, perm);
            swap_map_swappable(mm, addr, page, 1);
            page->pra_vaddr = addr;
        }
        else
        {
            // swap未初始化，但PTE不为0
            // 检查是否是有效页面（PTE_V位）
            if (*ptep & PTE_V)
            {
                // 页面已经存在，可能是权限问题，直接返回成功
                return 0;
            }
            // 不是有效页面，可能是残留的swap条目或无效数据
            // 清除PTE并分配新页面
            *ptep = 0;
            if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL)
            {
                cprintf("pgdir_alloc_page in do_pgfault failed (swap not ready)\n");
                return -E_NO_MEM;
            }
        }
    }

    return 0;
}
```

接下来我们看看FIFO页面置换算法是怎么在ucore里实现的。
