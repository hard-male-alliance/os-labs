> 来源：[项目组成](http://8.135.34.58/lab2026/_book/lab2/lab2_2_2_files.html)

<span id="项目组成"></span>

### 项目组成

表1：实验二文件列表

``` text
.
├── Makefile
├── kern
│   ├── debug
│   │   ├── assert.h
│   │   └── panic.c
│   ├── driver
│   │   ├── console.c
│   │   ├── console.h
│   │   ├── dtb.c
│   │   └── dtb.h
│   ├── init
│   │   ├── entry.S
│   │   └── init.c
│   ├── libs
│   │   └── stdio.c
│   └── mm
│       ├── best_fit_pmm.c
│       ├── best_fit_pmm.h
│       ├── default_pmm.c
│       ├── default_pmm.h
│       ├── memlayout.h
│       ├── mmu.h
│       ├── pmm.c
│       └── pmm.h
├── lab2.md
├── libs
│   ├── defs.h
│   ├── error.h
│   ├── list.h
│   ├── printfmt.c
│   ├── readline.c
│   ├── riscv.h
│   ├── sbi.c
│   ├── sbi.h
│   ├── stdarg.h
│   ├── stdio.h
│   ├── string.c
│   └── string.h
└── tools
    ├── boot.ld
    ├── function.mk
    ├── gdbinit
    ├── grade.sh
    ├── kernel.ld
    └── kernel_nopage.ld
```

**编译方法**

编译并运行代码的命令如下：

``` text
make qemu
```

完成实验任务并将物理内存管理器切换为 Best-Fit 后，可以得到类似如下的输出（具体地址可能因代码大小和运行环境而有所不同）：

``` text
+ ld bin/kernel
riscv64-unknown-elf-objcopy bin/kernel --strip-all -O binary bin/ucore.img

OpenSBI v0.4 (Jul  2 2019 11:53:53)
   ____                    _____ ____ _____
  / __ \                  / ____|  _ \_   _|
 | |  | |_ __   ___ _ __ | (___ | |_) || |
 | |  | | '_ \ / _ \ '_ \ \___ \|  _ < | |
 | |__| | |_) |  __/ | | |____) | |_) || |_
  \____/| .__/ \___|_| |_|_____/|____/_____|
        | |
        |_|

Platform Name          : QEMU Virt Machine
Platform HART Features : RV64ACDFIMSU
Platform Max HARTs     : 8
Current Hart           : 0
Firmware Base          : 0x80000000
Firmware Size          : 112 KB
Runtime SBI Version    : 0.1

PMP0: 0x0000000080000000-0x000000008001ffff (A)
PMP1: 0x0000000000000000-0xffffffffffffffff (A,R,W,X)
DTB Init
HartID: 0
DTB Address: 0x<runtime-dependent>
Physical Memory from DTB:
  Base: 0x0000000080000000
  Size: 0x0000000008000000 (128 MB)
  End:  0x0000000087ffffff
DTB init completed
(THU.CST) os is loading ...
Special kernel symbols:
  entry  0xffffffffc02000c0 (virtual)
  etext  0xffffffffc02011de (virtual)
  edata  0xffffffffc0205008 (virtual)
  end    0xffffffffc0205058 (virtual)
Kernel executable memory footprint: 20KB
memory management: best_fit_pmm_manager
physical memory map:
  memory: 0x0000000008000000, [0x0000000080000000, 0x0000000087ffffff].
check_alloc_page() succeeded!
satp virtual address: 0xffffffffc0204000
satp physical address: 0x0000000080204000
```

从输出可以看到，ucore依次显示入口地址、各段结束地址和内核镜像结束地址，然后输出设备树探测到的物理内存范围，并运行物理页分配检查。需要注意的是，`entry.S`在进入`kern_init()`之前已经建立启动页表并开启 Sv39 分页；这里的物理内存初始化是在分页模式下建立`Page`描述符和初始空闲页区间。
