> 来源：[项目组成](http://8.135.34.58/lab2026/_book/lab5/lab5_2_2_files.html)

<span id="项目组成"></span>

### 项目组成

``` text
├── boot
├── kern
│ ├── debug
│ │ ├── kdebug.c
│ │ └── ...
│ ├── mm
│ │ ├── memlayout.h
│ │ ├── pmm.c
│ │ ├── pmm.h
│ │ ├── vmm.c
│ │ └── vmm.h
│ ├── process
│ │ ├── proc.c
│ │ ├── proc.h
│ │ └── ...
│ ├── schedule
│ │ ├── sched.c
│ │ └── ...
│ ├── syscall
│ │ ├── syscall.c
│ │ └── syscall.h
│ └── trap
│   ├── trap.c
│   ├── trapentry.S
│   ├── trap.h
│   └── vectors.S
├── libs
├── tools
└── user
```

相对于实验4，lab5 主要增加了用户进程、系统调用和用户态到内核态的切换链路，因此改动重点集中在 `mm`、`process`、`syscall` 和 `trap` 这几部分。

`kern/mm/` 与本次实验关系最直接。`memlayout.h` 补充了用户虚拟地址空间相关的布局定义；`pmm.[ch]` 扩展了页表项管理和页复制/回收能力，支持 `do_fork` 的内存复制以及 `do_exit` 的地址空间释放；`vmm.[ch]` 则补齐了 `mm_struct`、`vma_struct` 相关操作，包括合法虚拟地址区间管理、父子进程地址空间复制，以及用户地址空间与内核地址空间之间的安全拷贝检查。

`kern/process/` 是用户进程生命周期的核心。`proc.[ch]` 扩展了 `proc_struct`，并补齐了进程创建、复制、执行、退出、等待和调度相关的逻辑，包括 `alloc_proc`、`proc_init`、`kernel_thread`、`do_fork`、`copy_mm`、`do_execve`、`load_icode`、`do_exit`、`do_yield`、`do_wait`、`do_kill`、`cpu_idle` 等函数。这些函数把“创建进程、装载程序、复制地址空间、回收资源、回到调度器”连成了一条完整链路。

`kern/syscall/` 负责把用户态请求转发到内核处理函数。`syscall.c` 读取系统调用号和参数，并分发到 `do_exit`、`do_fork`、`do_wait`、`do_execve`、`do_yield`、`do_kill`、`getpid`、`putc`、`pgdir` 等对应入口，使用户态能通过统一接口获得内核服务。

`kern/trap/` 负责异常和中断的入口与返回。`trap.c` 识别 `ecall` 等异常来源，并将用户态系统调用转发给 `syscall`；`trapentry.S` 则负责保存和恢复上下文，保证用户态与内核态切换时寄存器和栈状态保持一致。对于 lab5 来说，这部分是系统调用能否顺利返回用户态的关键。

`kern/schedule/` 负责调度。`sched.c` 通过 `schedule` 和 `wakeup_proc` 等接口，把处于可运行状态的进程选出来并切换到 CPU 上执行。`proc_run` 与底层上下文切换逻辑配合，完成进程之间的切换。

`user/` 目录下新增了用户程序和用户库。用户程序用于验证 `fork`、`exec`、`wait`、`exit`、`yield` 等系统调用是否正确；用户库则把这些接口包装成普通 C 函数，并通过系统调用进入内核。
