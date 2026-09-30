> 来源：[练习](http://8.135.34.58/lab2026/_book/lab4/lab4_2_1_exercises.html)

<span id="练习"></span>

### 练习

本实验按“虚拟内存页操作 -\> 进程创建与初始化 -\> 进程调度”三条主线展开，核心目标是让内核从“已经具备物理内存管理和基础页表能力”进一步走到“能够创建线程、登记线程、调度线程并完成上下文切换”。对应的执行链可以概括为 `kern_init -> pmm_init -> pic_init/idt_init -> vmm_init -> proc_init -> cpu_idle -> schedule -> proc_run -> switch_to`，其中前半段负责把地址空间准备好，后半段负责让第一个可运行线程真正跑起来。

本实验依赖实验2/3。请使用你此前设计的提示词生成相应实验代码，并将生成结果填写到本实验代码中标有“lab2”/“lab3”注释的对应位置，确保代码能够正确编译。

<span id="功能模块一：虚拟内存页操作功能（设计提示词借助-ai-完成）"></span>

#### 功能模块一：虚拟内存页操作功能（设计提示词借助 ai 完成）

要求实现的函数有：

``` text
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);(kern/mm/pmm.c)
void page_remove(pde_t *pgdir, uintptr_t la);(kern/mm/pmm.c)
```

整个功能模块的具体功能为：这个模块负责在现有多级页表框架上完成最基本的映射维护，也就是把物理页挂到指定线性地址上，以及把已有映射安全地撤销掉。`page_insert` 负责建立或更新映射，`page_remove` 负责删除映射，它们共同决定一个线性地址最终对应哪一页物理内存，同时也决定物理页引用计数是否正确、页表状态是否一致、TLB 是否需要失效。这个模块完成后，页表测试、静态内核映射检查以及后续线程地址空间相关逻辑才有可靠基础。

整个功能模块的注意事项为：这里最关键的是保持“页表项内容、物理页引用计数、TLB 状态”三者同步。`page_insert` 不能简单覆盖旧项，而要正确处理原地址已经映射到同一物理页、或者原地址已经映射到其他物理页这两种情况；`page_remove` 也不能假定目标地址一定存在有效映射。由于 `get_pte(pgdir, la, 1)` 可能需要分配中间级页表页，所以映射建立过程本身可能失败，调用路径必须能正确返回错误并保持系统状态干净。所有页表更新完成后都要考虑对应 TLB 失效，否则测试中会出现看似已更新、实际上仍在使用旧翻译结果的情况。

整个功能模块应该满足的对外的接口为：对外只需要提供“建立映射”和“撤销映射”两个动作。`page_insert` 成功时应让指定线性地址稳定映射到目标物理页，并把权限位设置为调用者要求的状态；失败时应返回 `-E_NO_MEM`，且不能留下半完成映射。`page_remove` 则应作为一个幂等删除接口存在，目标地址有映射时删除映射、无映射时安静返回，不应把“未映射地址”当作异常情况。

整个功能模块可以借助的接口函数、结构体、数据结构为：这个模块主要依赖 `get_pte()`、`pte_create()`、`pte2page()`、`page_ref_inc()`、`page_ref_dec()`、`set_page_ref()`、`alloc_page()`、`free_page()`、`tlb_invalidate()`、`flush_tlb()` 等现成接口，也依赖 `pde_t`、`pte_t`、`struct Page`、`pages`、`boot_pgdir_va` 以及页表权限位宏 `PTE_V`、`PTE_U`、`PTE_W`、`PTE_R`。其中 `get_pte()` 负责定位目标页表项，后续两个函数则在此基础上完成映射建立和映射删除。

<span id="功能模块二：进程创建及初始化功能（设计提示词借助-ai-完成）"></span>

#### 功能模块二：进程创建及初始化功能（设计提示词借助 ai 完成）

要求实现的函数有：

``` text
static struct proc_struct *alloc_proc(void);(kern/process/proc.c)
void proc_init(void);(kern/process/proc.c)
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);(kern/process/proc.c)
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);(kern/process/proc.c)
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);(kern/process/proc.c)
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);(kern/process/proc.c)
```

整个功能模块的具体功能为：这个模块负责把系统中“已经能运行内核代码”推进到“已经能创建并登记多个内核线程”。`alloc_proc` 负责得到一个干净的进程控制块，`proc_init` 负责把当前执行现场包装成 `idleproc` 并创建第一个真正的内核线程 `initproc`，`kernel_thread` 负责把函数入口和参数整理成线程首次运行所需的临时上下文，`do_fork` 负责把这些信息和内核栈、PID、链表、状态位等资源真正组合成一个可运行实体，`copy_thread` 负责把 trapframe 和 context 安放到新线程自己的内核栈上，而 `copy_mm` 则在当前实验阶段承担地址空间处理接口的占位作用。这个模块完成后，系统里至少要存在 `idleproc` 和 `initproc` 两个关键线程，并且 `initproc` 需要能够在后续调度中被真正切换执行。

整个功能模块的注意事项为：这里最容易出错的是“线程生命周期状态”和“资源挂接顺序”不一致。`alloc_proc` 必须把 PCB 置于安全空白状态，避免后续代码误用未初始化字段；`proc_init` 中的 `idleproc` 并不是普通 fork 出来的线程，而是直接复用当前执行现场，所以它的 PID、内核栈、调度标记和 `current` 位置都带有特殊含义；`kernel_thread` 与 `copy_thread` 必须配合 `forkret`、`forkrets`、`__trapret` 形成正确的首次运行链路；`do_fork` 则必须正确处理失败清理、PID 分配、链表挂接和唤醒顺序，否则会出现资源泄漏、僵尸结构或线程状态错误。当前阶段的 `copy_mm` 不做真正的用户地址空间复制，但仍需要维持函数形态完整，方便后续实验继续扩展。

整个功能模块应该满足的对外的接口为：这个模块提供的是“线程从哪里来、如何进入系统、如何第一次运行”的完整外部接口。`alloc_proc` 应返回一个已完成基础字段初始化的 PCB；`proc_init` 执行后应保证进程管理全局结构、`idleproc`、`current` 和 `initproc` 都处于可用状态；`kernel_thread` 应把普通函数包装成可创建线程的上下文并返回新线程 PID；`do_fork` 应返回新线程 PID 或负错误码；`copy_thread` 与 `copy_mm` 则作为内部辅助接口，分别负责首次运行现场和地址空间处理。

整个功能模块可以借助的接口函数、结构体、数据结构为：这个模块主要依赖 `struct proc_struct`、`struct context`、`struct trapframe`、`list_entry_t`、`proc_list`、`hash_list`、`idleproc`、`initproc`、`current`、`nr_process` 等核心数据结构和全局变量，也依赖 `kmalloc()`、`kfree()`、`memset()`、`memcpy()`、`alloc_pages()`、`free_pages()`、`page2kva()`、`kva2page()`、`set_proc_name()`、`find_proc()`、`get_pid()`、`wakeup_proc()`、`list_init()`、`list_add()`、`boot_pgdir_pa`、`bootstack`、`read_csr(sstatus)`、`SSTATUS_SPP`、`SSTATUS_SPIE`、`SSTATUS_SIE`、`kernel_thread_entry`、`forkret`、`forkrets` 等已有接口与符号。

<span id="功能模块三：进程调度（设计提示词借助-ai-完成）"></span>

#### 功能模块三：进程调度（设计提示词借助 ai 完成）

要求实现的函数有：

``` text
void cpu_idle(void);(kern/process/proc.c)
void schedule(void);(kern/schedule/sched.c)
void proc_run(struct proc_struct *proc);(kern/process/proc.c)
void switch_to(struct context *from, struct context *to);(kern/process/switch.s)
```

整个功能模块的具体功能为：这个模块负责把已经创建出来的内核线程真正交给 CPU 执行。`cpu_idle` 作为空闲线程的执行主体，持续等待调度需求；`schedule` 负责从进程链表中选出下一个可运行线程；`proc_run` 负责在安全的临界区内切换当前线程、页表和执行现场；`switch_to` 则负责最底层的寄存器上下文保存与恢复。这个模块完成后，系统不再只是“拥有线程对象”，而是已经具备让 `initproc` 从 `idleproc` 手中接管 CPU 的能力。

整个功能模块的注意事项为：这里最重要的是切换过程必须保持原子性。`schedule` 和 `proc_run` 都需要在关键区域内避免被中断打断，否则当前线程、页表和实际执行现场之间可能出现不一致。`schedule` 负责选择正确的 runnable 线程，并在找不到可运行线程时退回 `idleproc`；`proc_run` 负责更新 `current`、切换页表并调用 `switch_to`；`switch_to` 只做上下文保存和恢复，不参与调度策略判断。新线程第一次运行时之所以能走到正确入口，是因为前一个模块已经把它的 `context.ra` 和 `trapframe` 预先组织成了 `forkret -> forkrets -> __trapret` 这条恢复链路。

整个功能模块应该满足的对外的接口为：对外而言，`cpu_idle` 应持续作为空闲循环存在，检测到需要重调度时调用 `schedule`；`schedule` 应完成一次合法的调度选择并触发进程切换；`proc_run` 应让目标线程成为当前运行线程并完成页表切换；`switch_to` 则应根据两个 `struct context` 指针完成从旧线程到新线程的寄存器状态迁移。

整个功能模块可以借助的接口函数、结构体、数据结构为：这个模块主要依赖 `struct proc_struct`、`struct context`、`proc_list`、`current`、`idleproc`、`need_resched`、`runs`、`PROC_RUNNABLE`、`local_intr_save()`、`local_intr_restore()`、`lsatp()`、`switch_to()`、`list_next()`、`le2proc()` 以及 `forkret`、`forkrets`、`__trapret` 这一条首次运行恢复链路。

<span id="扩展练习-challenge"></span>

#### 扩展练习 Challenge

1.  说明语句 `local_intr_save(intr_flag); ... local_intr_restore(intr_flag);` 是如何实现开关中断的。
2.  深入理解不同分页模式的工作原理（思考题）。

`get_pte()` 函数位于 `kern/mm/pmm.c`，用于在页表中查找或创建页表项，从而实现对指定线性地址对应的物理页的访问和映射操作。这在操作系统的分页机制下，是虚拟内存与物理内存建立映射关系的关键基础。

- `get_pte()` 函数中有两段形式类似的代码，结合 `sv32`、`sv39`、`sv48` 的异同，解释这两段代码为什么如此相像。
- 目前 `get_pte()` 函数将页表项查找和页表页分配合并在一个函数里，你认为这种写法好吗？有没有必要把两个功能拆开？
