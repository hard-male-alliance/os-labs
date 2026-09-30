> 来源：[物理内存探测](http://8.135.34.58/lab2026/_book/lab2/lab2_3_5_2_search_phymem_layout.html)

<span id="物理内存探测"></span>

#### 物理内存探测

操作系统怎样知道物理内存所在的那段物理地址呢？在 RISC-V 中，这个一般是由 bootloader ，即 OpenSBI 来完成的。它来完成对于包括物理内存在内的各外设的扫描，将扫描结果以 DTB(Device Tree Blob) 的格式保存在物理内存中的某个地方。随后 OpenSBI 会将其地址保存在 `a1` 寄存器中，给我们使用。

这个扫描结果描述了所有外设的信息，当中也包括 Qemu 模拟的 RISC-V 计算机中的物理内存。

> 扩展 **Qemu 模拟的 RISC-V virt 计算机中的物理内存**
>
> 通过查看[virt.c](https://github.com/qemu/qemu/blob/master/hw/riscv/virt.c)的**virt_memmap\[\]**的定义，可以了解到 Qemu 模拟的 RISC-V virt 计算机的详细物理内存布局。可以看到，整个物理内存中有不少内存空洞（即含义为**unmapped**的地址空间），也有很多外设特定的地址空间，现在我们看不懂没有关系，后面会慢慢涉及到。目前只需关心最后一块含义为**DRAM**的地址空间，这就是 OS 将要管理的 128MB 的内存空间。
>
> | 起始地址   | 终止地址   | 含义                                                  |
> |:-----------|:-----------|:------------------------------------------------------|
> | 0x0        | 0x100      | QEMU VIRT_DEBUG                                       |
> | 0x100      | 0x1000     | unmapped                                              |
> | 0x1000     | 0x12000    | QEMU MROM (包括 hard-coded reset vector; device tree) |
> | 0x12000    | 0x100000   | unmapped                                              |
> | 0x100000   | 0x101000   | QEMU VIRT_TEST                                        |
> | 0x101000   | 0x2000000  | unmapped                                              |
> | 0x2000000  | 0x2010000  | QEMU VIRT_CLINT                                       |
> | 0x2010000  | 0x3000000  | unmapped                                              |
> | 0x3000000  | 0x3010000  | QEMU VIRT_PCIE_PIO                                    |
> | 0x3010000  | 0xc000000  | unmapped                                              |
> | 0xc000000  | 0x10000000 | QEMU VIRT_PLIC                                        |
> | 0x10000000 | 0x10000100 | QEMU VIRT_UART0                                       |
> | 0x10000100 | 0x10001000 | unmapped                                              |
> | 0x10001000 | 0x10002000 | QEMU VIRT_VIRTIO                                      |
> | 0x10002000 | 0x20000000 | unmapped                                              |
> | 0x20000000 | 0x24000000 | QEMU VIRT_FLASH                                       |
> | 0x24000000 | 0x30000000 | unmapped                                              |
> | 0x30000000 | 0x40000000 | QEMU VIRT_PCIE_ECAM                                   |
> | 0x40000000 | 0x80000000 | QEMU VIRT_PCIE_MMIO                                   |
> | 0x80000000 | 0x88000000 | DRAM 缺省 128MB，大小可配置                           |

那么我们就可以很方便的从a1寄存器中读取设备树数据存储地址，在kern_entry的开头将设备树数据从a1寄存器中读取出来，并存入全局变量`boot_dtb`中（顺便读取了当前cpu核心号）

``` text
# kern\init\entry.S
# a0: hartid
# a1: dtb physical address
# save hartid and dtb address
la t0, boot_hartid
sd a0, 0(t0)
la t0, boot_dtb
```

在kern_entry部分的初始化结束，我们正式进入到kern_init之后，会执行dtb_init函数来读取设备树结构中储存的相关信息。对设备树结构感兴趣的可以点击[链接](https://blog.csdn.net/Rank_d/article/details/106289183)了解

``` text
// kern\init\init.c
int kern_init(void) {
    extern char edata[], end[];
    // 先清零 BSS，再读取并保存 DTB 中的内存信息
    memset(edata, 0, end - edata);
    dtb_init();
    // 其他初始化
}
// kern\driver\dtb.c
// 保存解析出的系统物理内存信息
static uint64_t memory_base = 0;
static uint64_t memory_size = 0;

void dtb_init(void) {
    cprintf("DTB Init\n");
    cprintf("HartID: %ld\n", boot_hartid);
    cprintf("DTB Address: 0x%lx\n", boot_dtb);

    if (boot_dtb == 0) {
        cprintf("Error: DTB address is null\n");
        return;
    }

    // 转换为虚拟地址
    uintptr_t dtb_vaddr = boot_dtb + PHYSICAL_MEMORY_OFFSET;
    const struct fdt_header *header = (const struct fdt_header *)dtb_vaddr;

    // 验证DTB
    uint32_t magic = fdt32_to_cpu(header->magic);
    if (magic != 0xd00dfeed) {
        cprintf("Error: Invalid DTB magic number: 0x%x\n", magic);
        return;
    }

    // 提取内存信息
    uint64_t mem_base, mem_size;
    if (extract_memory_info(dtb_vaddr, header, &mem_base, &mem_size) == 0) {
        cprintf("Physical Memory from DTB:\n");
        cprintf("  Base: 0x%016lx\n", mem_base);
        cprintf("  Size: 0x%016lx (%ld MB)\n", mem_size, mem_size / (1024 * 1024));
        cprintf("  End:  0x%016lx\n", mem_base + mem_size - 1);
        // 保存到全局变量，供 PMM 查询
        memory_base = mem_base;
        memory_size = mem_size;
    } else {
        cprintf("Warning: Could not extract memory info from DTB\n");
    }
    cprintf("DTB init completed\n");
}
```

由此，我们就已经将内存的起点和大小读取到了全局变量`memory_base`和`memory_size`中，我们会在物理内存管理初始化的时候用到这些信息

``` text
// kern\mm\pmm.c
void pmm_init(void) {
    // other things
    page_init();
    // other things
}
```

`page_init()`连接了设备树探测结果和物理页分配器。它需要读取`get_memory_base()`与`get_memory_size()`保存的信息，确认内存范围有效并输出探测结果，再结合内核能够管理的地址上界确定物理页号范围。但是实现这个函数是本实验任务之一，这里就不展示其完整函数体，而是说明实现时必须满足的内存布局关系。

Qemu 规定的 DRAM 物理内存的起始物理地址为 `0x80000000` 。而在 Qemu 中，可以使用 `-m` 指定 RAM 的大小，默认是 `128MiB` 。因此，默认的 DRAM 物理内存地址范围就是 `[0x80000000,0x88000000)` 。

但是，有一部分 DRAM 空间已经被占用，不能用来存别的东西了！

- 物理地址空间 `[0x80000000,0x80200000)` 被 OpenSBI 占用；
- 物理地址空间 `[0x80200000,KernelEnd)` 被内核各代码与数据段占用；
- 其实设备树扫描结果 DTB 还占用了一部分物理内存，不过我们目前只在初始化的时读取其中的内存起点和长度信息，所以之后可以将它所占用的空间用来存别的东西。

暂不考虑管理数据本身的占用时，可供内核继续规划的物理内存候选范围是`[KernelEnd, 0x88000000)`。这里的`KernelEnd`为内核代码结尾的物理地址，而`kernel.ld`中定义的`end`符号表示内核代码结尾的虚拟地址，二者不能直接混用。

为了管理物理内存，内核需要使用`Page`结构体记录每个物理页的状态。所有`Page`描述符连续排列在内核镜像之后并形成数组，这个数组本身也要占用物理内存，因此内核镜像和描述符数组所在的页面都不能加入空闲页分配器。`page_init()`负责建立这种对应关系，并把描述符数组之后剩余的完整页面交给物理内存管理器。

可以把初始化后的物理内存布局理解为：OpenSBI 占用区之后依次是内核镜像、必要的对齐空隙、`Page`描述符数组、再次按页对齐产生的空隙，以及最终可以分配的连续物理页。物理内存末尾不足一页的部分同样不能交给分配器。

``` text
// kern/mm/pmm.h

/* *
 * PADDR - takes a kernel virtual address (an address that points above
 * KERNBASE),
 * where the machine's maximum 128MB of physical memory is mapped and returns
 * the
 * corresponding physical address.  It panics if you pass it a non-kernel
 * virtual address.
 * */
#define PADDR(kva)                                                 \
    ({                                                             \
        uintptr_t __m_kva = (uintptr_t)(kva);                      \
        if (__m_kva < KERNBASE) {                                  \
            panic("PADDR called with invalid kva %08lx", __m_kva); \
        }                                                          \
        __m_kva - va_pa_offset;                                    \
    })

/* *
 * KADDR - takes a physical address and returns the corresponding kernel virtual
 * address. It panics if you pass an invalid physical address.
 * */
/*
#define KADDR(pa)                                                \
    ({                                                           \
        uintptr_t __m_pa = (pa);                                 \
        size_t __m_ppn = PPN(__m_pa);                            \
        if (__m_ppn >= npage) {                                  \
            panic("KADDR called with invalid pa %08lx", __m_pa); \
        }                                                        \
        (void *)(__m_pa + va_pa_offset);                         \
    })
*/
extern struct Page *pages;
extern size_t npage;
```

``` text
// kern/mm/pmm.c

// pages指针保存的是第一个Page结构体所在的位置，也可以认为是Page结构体组成的数组的开头
// 由于C语言的特性，可以把pages作为数组名使用，pages[i]表示顺序排列的第i个结构体
struct Page *pages;
size_t npage = 0;
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
const size_t nbase = DRAM_BASE / PGSIZE;
//(npage - nbase)表示物理内存的页数
```

`npage`表示内核可管理的物理页号上界，`nbase`表示 DRAM 起始位置对应的物理页号，因此描述符数组需要覆盖`[nbase, npage)`中的每个物理页。`pages`保存描述符数组的虚拟地址，数组起点应位于`end`之后并按页对齐；数组末尾则需要借助`PADDR`转换为物理地址，才能继续计算真正可用的物理内存起点。

在空闲区建立之前，所有描述符都应先处于保留状态，防止尚未完成布局的页面被提前分配。描述符数组之后的地址向上按页对齐，设备树报告的内存末端向下按页对齐，只有两者之间非空的完整页面区间才可以通过`pa2page()`转换为描述符起点，并交给`init_memmap()`。这样既能避开内核和描述符数组，也不会把不完整的尾页加入空闲区。

`page_init()`完成上述工作时会调用`init_memmap()`，这和另一个结构体`pmm_manager`有关。虽然 C 语言不直接支持面向对象，但我们可以把物理内存管理功能集中到一个包含函数指针的结构体中。这里的`init_memmap()`实际上会继续调用当前`pmm_manager`提供的初始化接口。

``` text
// kern/mm/pmm.c

// physical memory management
const struct pmm_manager *pmm_manager;


// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}
```

``` text
// kern/mm/pmm.h
#ifndef __KERN_MM_PMM_H__
#define __KERN_MM_PMM_H__

#include <assert.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>

// pmm_manager is a physical memory management class. A special pmm manager -
// XXX_pmm_manager
// only needs to implement the methods in pmm_manager class, then
// XXX_pmm_manager can be used
// by ucore to manage the total physical memory space.
struct pmm_manager {
    const char *name;  // XXX_pmm_manager's name
    void (*init)(
        void);  // 初始化XXX_pmm_manager内部的数据结构（如空闲页面的链表）
    void (*init_memmap)(
        struct Page *base,
        size_t n);  //知道了可用的物理页面数目之后，进行更详细的初始化
    struct Page *(*alloc_pages)(
        size_t n);  // 分配至少n个物理页面, 根据分配算法可能返回不同的结果
    void (*free_pages)(struct Page *base, size_t n);  // free >=n pages with
                                                      // "base" addr of Page
                                                      // descriptor
                                                      // structures(memlayout.h)
    size_t (*nr_free_pages)(void);  // 返回空闲物理页面的数目
    void (*check)(void);            // 测试正确性
};

extern const struct pmm_manager *pmm_manager;

void pmm_init(void);

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void); // number of free pages

#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)
```

pmm_manager提供了各种接口：分配页面，释放页面，查看当前空闲页面数。但是我们好像始终没看见pmm_manager内部对这些接口的实现，其实是因为那些接口只是作为函数指针，作为pmm_manager的一部分，我们需要把那些函数指针变量赋值为真正的函数名称。

还记得最早我们在`pmm_init()`里首先调用了`init_pmm_manager()`, 在这里面我们把pmm_manager的指针赋值成`&default_pmm_manager`， 看起来我们在这里实现了那些接口。

``` text
// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}
// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}
```

到现在，我们距离完整的内存管理， 就只差`default_pmm_manager`结构体的实现了，也就是我们要在里面实现页面分配算法。
