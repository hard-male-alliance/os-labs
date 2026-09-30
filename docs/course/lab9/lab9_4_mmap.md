> 来源：[内存映射机制的实现](http://8.135.34.58/lab2026/_book/lab9/lab9_4_mmap.html)

<span id="内存映射机制的实现"></span>

## 内存映射机制的实现

<span id="mmap的基本原理与设计"></span>

### mmap的基本原理与设计

什么是mmap？简单地说，`mmap`（Memory Map）是一种内存映射机制，它允许将文件或其他对象映射到进程的虚拟地址空间中。通过mmap，进程可以像访问普通内存一样访问文件内容，而不需要使用传统的`read`/`write`系统调用。

在传统的文件I/O模型中，当进程需要读取文件内容时，需要调用`read`系统调用。这个过程涉及多次数据拷贝：首先，内核从磁盘读取数据到内核缓冲区（页缓存）；然后，内核将数据从内核缓冲区拷贝到用户空间的缓冲区。写操作同样需要经历相反的拷贝过程。这种方式存在几个问题：一是每次读写都需要系统调用的开销；二是数据需要在内核空间和用户空间之间拷贝，消耗CPU时间和内存带宽；三是对于随机访问模式，需要频繁调用`lseek`来定位文件位置。

mmap提供了一种不同的思路。它将文件直接映射到进程的虚拟地址空间，使得文件内容可以通过普通的内存访问指令（如load/store）来读写。当进程访问映射区域时，如果对应的物理页面不存在，会触发缺页异常，此时内核才从磁盘读取数据到物理内存，并建立页表映射。这种方式的优势在于：减少了数据拷贝（数据可以直接在用户空间访问，无需从内核缓冲区拷贝）；减少了系统调用次数（只需要在建立和解除映射时调用mmap/munmap）；对于随机访问模式更加友好（可以直接通过指针偏移访问任意位置）。

mmap的核心思想是**延迟加载**（Lazy Loading）和**按需分页**（Demand Paging）。当调用mmap时，内核只是建立虚拟地址到文件的映射关系，并不立即分配物理内存或读取文件内容。只有当进程首次访问映射区域时，才会触发**缺页异常**（Page Fault），此时内核才真正分配物理页面，并从文件中读取相应的数据。这种机制使得即使映射一个很大的文件，也不会立即占用大量物理内存，只有实际访问到的部分才会被加载。

mmap支持两种映射类型：**文件映射**（File-backed Mapping）将文件内容映射到虚拟地址空间，缺页时从文件读取数据，这是mmap最常见的用途；**匿名映射**（Anonymous Mapping）不关联任何文件，缺页时分配零页，常用于动态内存分配（如malloc在分配大块内存时会使用匿名mmap而不是brk）。

在之前的实验中，我们已经使用`vma_struct`来描述虚拟内存区域，`mm_struct`来管理进程的所有VMA。为了支持mmap的文件映射功能，我们需要在`vma_struct`中新增两个字段：

``` text
struct vma_struct {
    // ... 原有字段保持不变 ...
    int vm_pgoff;             // 文件映射的页偏移
    struct file *vm_file;     // 映射的文件指针（匿名映射为NULL）
};
```

`vm_file`指向被映射的文件结构体，对于匿名映射则为NULL。`vm_pgoff`记录映射在文件中的起始页偏移，用于在缺页时计算应该从文件的哪个位置读取数据。在`vma_create`函数中，这两个新字段被初始化为空值。

<span id="mmap系统调用的实现"></span>

### mmap系统调用的实现

用户态通过`mmap`和`munmap`系统调用来使用内存映射功能。在`user/libs/syscall.c`中，`sys_mmap`接收映射地址、长度、保护标志、映射标志、文件描述符和偏移量等参数，通过`ecall`指令陷入内核。

mmap使用两组标志位来控制映射的行为。保护标志（prot）定义在`libs/unistd.h`中，用于指定映射区域的访问权限：

``` text
// libs/unistd.h

/* mmap protection flags */
#define PROT_NONE           0x0         // 页面不可访问
#define PROT_READ           0x1         // 页面可读
#define PROT_WRITE          0x2         // 页面可写
#define PROT_EXEC           0x4         // 页面可执行

/* mmap flags */
#define MAP_SHARED          0x01        // 共享映射，修改对其他进程可见
#define MAP_PRIVATE         0x02        // 私有映射，修改不影响原文件
#define MAP_FIXED           0x10        // 精确使用指定地址
#define MAP_ANONYMOUS       0x20        // 匿名映射，不关联文件
```

内核态的`sysfile_mmap`函数（位于`kern/fs/sysfile.c`）负责处理这个系统调用。它首先将用户态的保护标志转换为内核的VMA标志——`PROT_READ`对应`VM_READ`，`PROT_WRITE`对应`VM_WRITE`，`PROT_EXEC`对应`VM_EXEC`。这些VMA标志定义在`kern/mm/vmm.h`中，用于在缺页处理时设置页表项的权限位。对于文件映射，还需要验证文件描述符的有效性并获取对应的`file`结构体。最后调用`do_mmap`完成实际的映射工作。

<span id="缺页异常中的mmap处理"></span>

### 缺页异常中的mmap处理

在前面的章节中，我们已经介绍了`do_pgfault`函数的整体框架，它负责处理各种类型的缺页异常。对于mmap映射的区域，缺页处理是其中的一个分支。当进程访问mmap映射的区域时，由于物理页面尚未分配，会触发缺页异常。在`do_pgfault`中，通过检查VMA的`vm_file`字段来判断是否为文件映射：如果`vm_file`不为NULL，则进入文件映射的缺页处理分支。

对于文件映射的缺页处理，函数分配一个物理页面，先将其清零，然后计算文件偏移量——文件偏移由VMA的页偏移（`vm_pgoff`）加上当前页相对于映射起始的偏移组成。接着通过`file_seek`定位到正确位置，用`file_read`读取一页数据到物理页面中。为了不影响其他文件操作，读取前后需要保存和恢复文件游标位置。对于匿名映射的缺页处理则与普通的缺页处理相同，只需分配一个物理页面即可，`pgdir_alloc_page`会自动将页面清零。

``` text
// kern/mm/vmm.c

int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr) {
    struct vma_struct *vma = find_vma(mm, addr);
    if (vma == NULL || vma->vm_start > addr) {
        return -E_INVAL;
    }

    uint32_t perm = PTE_U | PTE_R;
    if (vma->vm_flags & VM_WRITE) perm |= PTE_W;
    addr = ROUNDDOWN(addr, PGSIZE);

    pte_t *ptep = get_pte(mm->pgdir, addr, 1);

    // 文件映射缺页处理
    if (vma->vm_file != NULL && *ptep == 0) {
        struct Page *page = pgdir_alloc_page(mm->pgdir, addr, perm);
        void *kva = page2kva(page);
        memset(kva, 0, PGSIZE);

        size_t page_index = (addr - vma->vm_start) / PGSIZE;
        off_t file_offset = (vma->vm_pgoff + page_index) * PGSIZE;

        off_t saved_pos = vma->vm_file->pos;
        file_seek(vma->vm_file->fd, file_offset, LSEEK_SET);
        size_t read_size = 0;
        file_read(vma->vm_file->fd, kva, PGSIZE, &read_size);
        file_seek(vma->vm_file->fd, saved_pos, LSEEK_SET);
        return 0;
    }

    // 匿名页缺页处理
    if (*ptep == 0) {
        pgdir_alloc_page(mm->pgdir, addr, perm);
    }
    return 0;
}
```

测试程序`user/testmmap.c`包含了几个测试用例来验证mmap实现的正确性：匿名映射测试创建一页可读可写的匿名内存，写入数据后验证读取结果；多页映射测试创建4页连续内存，在每页写入不同数据来验证跨页访问和按需分页机制；文件映射测试打开一个文件并映射到内存，通过检查ELF文件头来验证文件内容是否正确读取；只读映射测试创建只读的匿名映射，验证读操作的正确性。

本节实现了一个简化版的mmap内存映射机制，通过扩展VMA结构来支持文件映射，采用延迟分配策略在缺页时才分配物理页面。虽然相比Linux的实现省略了共享映射、写时复制、部分解映射等高级特性，但已经展示了mmap的核心原理，为理解操作系统的内存管理机制提供了良好的基础。
