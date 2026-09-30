> 来源：[项目组成](http://8.135.34.58/lab2026/_book/lab4/lab4_2_2_files.html)

<span id="项目组成"></span>

### 项目组成

``` text
├── Makefile
├── kern
│   ├── debug
│   │   ├── assert.h
│   │   ├── kdebug.c
│   │   ├── kdebug.h
│   │   ├── kmonitor.c
│   │   ├── kmonitor.h
│   │   ├── panic.c
│   │   └── stab.h
│   ├── driver
│   │   ├── clock.c
│   │   ├── clock.h
│   │   ├── console.c
│   │   ├── console.h
│   │   ├── dtb.c
│   │   ├── dtb.h
│   │   ├── intr.c
│   │   ├── intr.h
│   │   ├── kbdreg.h
│   │   ├── picirq.c
│   │   └── picirq.h
│   ├── init
│   │   ├── entry.S
│   │   └── init.c
│   ├── libs
│   │   ├── readline.c
│   │   └── stdio.c
│   ├── mm
│   │   ├── default_pmm.c
│   │   ├── default_pmm.h
│   │   ├── kmalloc.c
│   │   ├── kmalloc.h
│   │   ├── memlayout.h
│   │   ├── mmu.h
│   │   ├── pmm.c
│   │   ├── pmm.h
│   │   ├── vmm.c
│   │   └── vmm.h
│   ├── process
│   │   ├── entry.S
│   │   ├── proc.c
│   │   ├── proc.h
│   │   └── switch.S
│   ├── schedule
│   │   ├── sched.c
│   │   └── sched.h
│   ├── sync
│   │   └── sync.h
│   └── trap
│       ├── trap.c
│       ├── trap.h
│       └── trapentry.S
├── libs
│   ├── atomic.h
│   ├── defs.h
│   ├── elf.h
│   ├── error.h
│   ├── hash.h
│   ├── list.h
│   ├── printfmt.c
│   ├── riscv.h
│   ├── sbi.h
│   ├── stdarg.h
│   ├── stdio.h
│   ├── stdlib.h
│   ├── string.c
│   └── string.h
└── tools
    ├── boot.ld
    ├── function.mk
    ├── gdbinit
    ├── grade.sh
    ├── kernel.ld
    ├── sign.c
    └── vector.c
```

相对与实验三，实验四中主要改动如下：

- kern/process/ （新增进程管理相关文件）

  - proc.\[ch\]：新增：实现进程、线程相关功能，包括：创建进程/线程，初始化进程/线程，处理进程/线程退出等功能
  - entry.S：新增：内核线程入口函数kernel_thread_entry的实现
  - switch.S：新增：上下文切换，利用堆栈保存、恢复进程上下文

- kern/init/

  - init.c：修改：完成虚拟内存管理初始化和进程系统初始化，并在内核初始化后切入idle进程

- kern/mm/

  - kmalloc.\[ch\]：新增：定义和实现了新的kmalloc/kfree函数。具体实现是基于slab分配的简化算法 （基本上与本次实验没有太直接的联系，了解kmalloc和kfree如何使用即可）。
  - memlayout.h：增加slab物理内存分配相关的定义与宏 （可不用理会）。
  - pmm.\[ch\]：修改：加入完整的页表管理功能（get_pte/page_insert/page_remove等），实现虚拟内存映射与地址转换；在pmm.c中添加了调用kmalloc_init函数,取消了老的kmalloc/kfree的实现；在pmm.h中取消了老的kmalloc/kfree的定义
  - vmm.\[ch\]：新增：定义并实现虚拟内存区域（VMA）管理，包括 mm_struct（内存管理结构）和 vma_struct（虚拟内存区域结构），提供 VMA 的创建、查找、插入和销毁等功能

- kern/trap/

  - trapentry.S：增加了汇编写的函数forkrets，用于do_fork调用的返回处理。

- kern/schedule/

  - sched.\[ch\]：新增：实现FIFO策略的进程调度
