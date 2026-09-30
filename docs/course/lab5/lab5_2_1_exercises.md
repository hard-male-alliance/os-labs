> 来源：[练习](http://8.135.34.58/lab2026/_book/lab5/lab5_2_1_exercises.html)

<span id="练习"></span>

### 练习

本章围绕“用户态如何获得内核服务”“第一个用户进程如何被启动”“用户进程如何被复制”“进程如何退出”四条主线展开。阅读时可以把整章理解成一条完整链路：用户程序先通过系统调用进入内核，内核完成进程管理与虚拟内存管理后，再通过调度把结果送回用户态继续执行。

本实验依赖实验2/3/4。请使用你此前设计的提示词生成相应实验代码，并将生成结果填写到本实验代码中标有“lab2”/“lab3”/“lab4”注释的对应位置，确保代码能够正确编译。

注意：原来lab3的 `RESTORE_ALL` 宏，它仅能用于纯内核、无用户态的系统。请在此基础上修改，扩充为支持用户态 / 内核态双向异常返回的完整版上下文恢复宏。

<span id="功能模块一：用户态系统调用功能（设计提示词借助ai完成）"></span>

#### 功能模块一：用户态系统调用功能（设计提示词借助ai完成）

需要更新和完善的函数有：

``` text
void exception_handler(struct trapframe *tf) (kern/trap/trap.c)
void syscall(void) (kern/syscall/syscall.c)
static int sys_exit(uint64_t arg[]) (kern/syscall/syscall.c)
static int sys_fork(uint64_t arg[]) (kern/syscall/syscall.c)
static int sys_wait(uint64_t arg[]) (kern/syscall/syscall.c)  
static int sys_exec(uint64_t arg[]) (kern/syscall/syscall.c)  
static int sys_yield(uint64_t arg[]) (kern/syscall/syscall.c)  
static int sys_kill(uint64_t arg[]) (kern/syscall/syscall.c)  
static int sys_getpid(uint64_t arg[]) (kern/syscall/syscall.c)  
static int sys_putc(uint64_t arg[]) (kern/syscall/syscall.c) 
static int sys_pgdir(uint64_t arg[]) (kern/syscall/syscall.c)
```

整个功能模块的具体功能为：这一部分负责把用户态通过 `ecall` 发起的请求接入内核，再把内核的处理结果写回到用户态寄存器中。`exception_handler` 这里只需要关注 `CAUSE_USER_ECALL` 这一类情况，把系统调用异常转交给 `syscall`，`syscall` 再从当前进程的 `trapframe` 中取出系统调用号和参数并完成分发，而各个 `sys_*` 函数则把统一编号的系统调用请求转换成具体内核语义，例如进程退出、进程复制、等待子进程、程序替换、让出 CPU、终止进程、获取当前进程号、向控制台输出字符以及查看页表等。整条链路的关键点是，用户库只负责把参数放进约定寄存器并触发异常，真正的语义判断、权限检查和资源更新都在内核态完成，最后由异常返回路径把结果带回用户程序继续执行。

整个功能模块的注意事项为：系统调用编号和参数位置必须与用户态封装保持一致，通常由 `a0` 保存系统调用号，`a1` 到 `a5` 保存参数；`exception_handler` 在识别到用户态系统调用后必须把 `epc` 前移，避免返回后重复执行同一条 `ecall`；`syscall` 的分发表必须保证边界正确，未实现或非法的编号不能被当作有效请求处理。凡是涉及用户指针的参数，都不能直接信任，必须结合 `user_mem_check` 和当前进程的地址空间做合法性判断；凡是会改变进程状态、进程树关系或页表内容的处理，都要和当前进程的 `trapframe`、`mm_struct`、`proc_struct` 保持同步，避免出现返回值已经写回但进程上下文没有切换好的情况。

整个功能模块应该满足的对外的接口为：用户态需要看到的是一组语义清晰的库函数接口，例如 `exit`、`fork`、`wait`、`waitpid`、`exec`、`yield`、`kill`、`getpid`、`cprintf` 以及页表查看相关接口；内核态需要提供统一的系统调用入口 `exception_handler` 和 `syscall`，并保证每个系统调用都能在返回时把结果正确放入用户可见的返回寄存器。对外表现上，这一模块应该让用户程序像在普通操作系统中那样使用系统调用，而不需要关心异常入口、寄存器传参和 trap 返回的细节。

整个功能模块可以借助的接口函数、结构体、数据结构为：`current` 和 `current->tf` 提供当前进程及其 trapframe；`trapframe`、`pushregs`、`sstatus`、`sepc`、`scause` 等结构和寄存器状态用于异常处理；`do_exit`、`do_fork`、`do_wait`、`do_execve`、`do_yield`、`do_kill` 是系统调用真正落到进程管理层的入口；`cputchar` 负责最终的字符输出；`print_trapframe` 便于定位异常；`user_mem_check` 用于检查用户参数是否合法；`libs/unistd.h` 和 `user/libs/syscall.c` 中定义的系统调用号和用户态封装则决定了整条调用链的入口形式。

这组函数合在一起，完成的是“异常入口 + 系统调用分发 + 内核语义落地 + 返回用户态”这一完整闭环。`exception_handler` 和 `syscall` 负责控制流转接，`sys_*` 负责把用户请求翻译成内核动作，而后续各个 `do_*` 函数则承担真正的进程管理和资源管理工作。

<span id="功能模块二：创建加载第一个用户进程功能（设计提示词借助ai完成）"></span>

#### 功能模块二：创建加载第一个用户进程功能（设计提示词借助ai完成）

需要更新和完善的函数有：

``` text
static int init_main(void *arg) (kern/process/proc.c)  
static struct proc_struct *alloc_proc(void) (kern/process/proc.c)  
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) (kern/process/proc.c)  
int do_wait(int pid, int *code_store) (kern/process/proc.c)  
static int user_main(void *arg) (kern/process/proc.c)  
static int kernel_execve(const char *name, unsigned char *binary, size_t size) (kern/process/proc.c)  
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size) (kern/process/proc.c)  
static int load_icode(unsigned char *binary, size_t size) (kern/process/proc.c)
```

整个功能模块的具体功能为：这一部分负责把系统从“只有内核线程在运行”推进到“第一个用户程序真正开始执行”。`proc_init` 先建立 `idleproc`，再由它创建 `init_main` 所在的内核线程；`init_main` 继续创建 `user_main`，并通过 `do_wait` 等待后续用户进程退出，保证第一个用户进程建立后，内核仍然能够收束整个启动流程。`user_main` 的职责不是长期执行业务逻辑，而是充当一个过渡内核线程，它通过嵌入到镜像中的用户程序二进制符号，调用 `kernel_execve` 把自己从内核线程转换成用户进程。`kernel_execve` 负责搭出从内核态回到用户态所需的执行上下文，`do_execve` 负责回收旧地址空间并触发新程序加载，`load_icode` 则负责解析 ELF、建立新的虚拟内存布局、装入代码段和数据段、创建用户栈并设置 trapframe，最终让 CPU 能从用户程序入口开始执行。

整个功能模块的注意事项为：这一条链路最容易出问题的地方是“上下文没有接上”。`kernel_execve` 不能只调用 `do_execve`，因为那样只完成了内存和进程映像的准备，却没有真正进入 trap 返回路径；`load_icode` 必须把用户态入口地址、用户栈指针和 `sstatus` 中的特权级信息设置正确，否则即使程序已经装入内存，也可能无法从 `sret` 正常回到用户态；`do_execve` 在替换当前进程映像时必须先处理旧 `mm` 的回收，再继续装载新程序，避免旧地址空间残留；`do_wait` 则要保证父子进程关系、僵尸态回收和睡眠唤醒逻辑一致，否则 `init_main` 可能无法正确等到用户进程退出。

整个功能模块应该满足的对外的接口为：从启动路径上看，`proc_init`、`kernel_thread`、`init_main`、`user_main`、`kernel_execve`、`do_execve`、`load_icode` 和 `do_wait` 组成了从内核启动到用户程序启动的完整对外接口；从用户可见行为上看，系统最终应该表现为第一个用户程序能够被正确装入、正确切换到用户态、并且在退出后被父进程正确等待和回收。`init_main` 还应该持续作为系统启动后的收尾控制点，确保用户进程链路结束后内核状态稳定。

整个功能模块可以借助的接口函数、结构体、数据结构为：`proc_struct`、`mm_struct`、`vma_struct`、`trapframe`、`elfhdr`、`proghdr`、`KERNEL_EXECVE` 与 `KERNEL_EXECVE2` 宏、`mm_map`、`dup_mmap`、`exit_mmap`、`page_insert`、`pgdir_alloc_page`、`lcr3/lsatp`、`user_mem_check`、`set_proc_name`、`wakeup_proc`、`schedule` 等接口都可能参与其中。`KSTACKSIZE`、`USTACKTOP`、`USTACKSIZE`、`SSTATUS_SPP`、`SSTATUS_SPIE`、`PTE_U`、`PTE_R/W/X/V` 等常量则决定了用户栈、权限位和 trapframe 的最终形态。

这一组函数共同构成的是“从内核线程过渡到第一个用户程序”的启动链。`init_main` 负责把启动过程拉到用户程序入口，`user_main` 负责选择真正要执行的用户二进制，`kernel_execve` 负责把执行流送进 trap 返回路径，`do_execve` 和 `load_icode` 负责完成地址空间替换与程序装载，而 `do_wait` 负责让启动阶段的父进程在逻辑上闭合。

<span id="功能模块三：复制用户进程功能（设计提示词借助ai完成）"></span>

#### 功能模块三：复制用户进程功能（设计提示词借助ai完成）

需要更新和完善的函数有：

``` text
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) (kern/process/proc.c)  
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc) (kern/process/proc.c)  
int dup_mmap(struct mm_struct *to, struct mm_struct *from) (kern/mm/vmm.c)  
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share) (kern/mm/pmm.c)
```

整个功能模块的具体功能为：这一部分负责把父进程的执行环境复制成一个新的子进程，使子进程拥有自己的进程控制块、内核栈、trapframe 和用户地址空间映像。`do_fork` 是整条链路的总入口，它需要把子进程创建出来并接入进程树，同时准备好后续调度所需的执行上下文；`copy_mm` 是在 `lab4` 已有实现基础上的更新点，它决定新进程是与父进程共享地址空间还是复制地址空间，在本实验的普通 fork 路径中主要体现为复制用户地址空间；`dup_mmap` 负责按 `vma` 粒度复制父进程的合法虚拟内存区域；`copy_range` 则负责按页粒度复制页面内容并建立新的页表映射。整体上，这条链路保证了子进程既能继承父进程看到的程序和数据，又能在之后独立运行、独立调度、独立退出。

整个功能模块的注意事项为：进程复制既要复制“看得见的地址空间”，也要复制“运行所需的上下文”，因此 `do_fork` 里不能只做页表复制，还要保证 `trapframe`、内核栈、进程关系和 PID 都正确建立。`copy_mm` 在处理 `mm_struct` 时要兼顾引用计数和复制路径，普通 fork 与共享地址空间的线程创建不能混为一谈；`dup_mmap` 复制 `vma` 时要保持区间顺序和权限一致，避免新进程得到非法或重叠的地址空间；`copy_range` 在复制页面时要确保源页内容完整、目标页映射有效、权限位与原页面语义一致，同时注意页表更新和引用计数的正确性，避免子进程和父进程在后续运行中互相破坏地址空间。凡是涉及进程树、哈希表、状态位和内存映射的修改，都应该在合适的临界区内完成。

整个功能模块应该满足的对外的接口为：从用户角度看，`fork` 最终应该表现为“父进程得到子进程 PID，子进程从 0 返回”的标准语义；从内核角度看，`do_fork`、`copy_mm`、`dup_mmap`、`copy_range` 必须把进程控制块、虚拟内存区域和实际页映射都复制完整，并保证新进程能够被调度器立即看到。对于内核线程场景，`do_fork` 还需要支持不复制用户地址空间的特殊路径，使得内核线程和用户进程的创建都能复用同一套框架。

整个功能模块可以借助的接口函数、结构体、数据结构为：`alloc_proc`、`setup_kstack`、`copy_thread`、`get_pid`、`set_links`、`hash_proc`、`wakeup_proc`、`mm_create`、`setup_pgdir`、`lock_mm/unlock_mm`、`find_vma`、`get_pte`、`page_insert`、`page2kva`、`pte2page`、`alloc_page`、`current`、`proc_list`、`hash_list`、`proc_struct`、`mm_struct`、`vma_struct`、`trapframe` 都会直接影响复制过程。它们共同提供了“创建新进程、复制地址空间、接入进程树、准备调度”的基础能力。

这组函数共同完成的是“从父进程生成子进程”的完整过程。`do_fork` 负责组织全局流程，`copy_mm` 负责决定是否共享或复制内存映像，`dup_mmap` 负责复制虚拟内存区间，`copy_range` 负责复制实际页内容并建立新的映射，最终让子进程在逻辑上继承父进程、在物理上拥有自己的执行实体。

<span id="功能模块四：其余系统调用功能（设计提示词借助ai完成）"></span>

#### 功能模块四：其余系统调用功能（设计提示词借助ai完成）

要求实现的函数有：

``` text
int do_exit(int error_code) (kern/process/proc.c)  
int do_yield(void) (kern/process/proc.c)  
int do_kill(int pid) (kern/process/proc.c)
```

整个功能模块的具体功能为：这一部分负责把用户态系统调用中最常见的三类进程控制动作落到内核中，分别是进程退出、主动让出 CPU 和终止指定进程。`do_exit` 负责处理当前进程结束时的资源回收与状态切换，它不仅要释放当前进程不再需要的用户地址空间，还要把进程标记成僵尸态，并通知父进程来完成最后的回收；`do_yield` 负责把当前进程标记为需要重新调度，让调度器尽快为它寻找新的运行机会；`do_kill` 则负责向目标进程发送退出意图，使其在合适的时机结束执行并进入退出流程。三者共同构成用户程序生命周期中的“结束、让出和终止”这条控制线。

整个功能模块的注意事项为：`do_exit` 不能让 `idleproc` 和 `initproc` 退出，且必须保证自己在变成僵尸前就已经完成必要的资源处理；进程退出时，父子关系、等待状态和僵尸回收都必须和 `do_wait` 的逻辑完全匹配，否则可能出现僵尸进程无人回收或者父进程永远睡眠的情况。`do_yield` 虽然实现简单，但它本质上是在影响调度器的决策，因此需要和 `need_resched`、当前进程状态以及中断时机保持一致；`do_kill` 在修改目标进程状态标志时也要考虑目标是否正在等待以及是否需要立即唤醒。凡是涉及进程状态转换的地方，都要避免在临界区之外留下半更新状态。

整个功能模块应该满足的对外的接口为：用户侧看到的是 `exit`、`yield`、`kill` 这类简单而清晰的进程控制接口；内核侧看到的是 `do_exit`、`do_yield`、`do_kill` 这三个与调度和资源回收紧密结合的处理入口。它们的共同目标不是单纯返回一个值，而是确保进程生命周期能在内核中闭合，并且和父进程等待、调度器切换、页面回收保持一致。

整个功能模块可以借助的接口函数、结构体、数据结构为：`current`、`schedule`、`wakeup_proc`、`user_mem_check`、`exit_mmap`、`put_pgdir`、`mm_destroy`、`unhash_proc`、`remove_links`、`local_intr_save/local_intr_restore`、`proc_struct`、`mm_struct`、`wait_state`、`PF_EXITING`、`PROC_ZOMBIE`、`WT_CHILD`、`WT_INTERRUPTED` 等接口和状态位都与这部分功能直接相关。它们共同保证退出、唤醒、等待和重新调度这几个动作能够在同一套进程状态机上协同工作。

这组函数共同完成的是“让进程结束并回到调度器”的收尾逻辑。`do_exit` 负责真正的退出与资源清理，`do_yield` 负责让出 CPU，`do_kill` 负责触发目标进程进入退出路径，三者与 `do_wait` 共同构成进程生命周期的闭环。

<span id="扩展练习-challenge"></span>

#### 扩展练习 Challenge

1.  实现 Copy on Write （COW）机制

    这个扩展练习涉及到本实验和上一个实验“虚拟内存管理”。在 ucore 操作系统中，当一个用户父进程创建自己的子进程时，父进程会把其申请的用户空间设置为只读，子进程可共享父进程占用的用户内存空间中的页面，这就是一个共享的资源。当其中任何一个进程修改此用户内存空间中的某页面时，ucore 会通过 page fault 异常获知该操作，并完成拷贝内存页面，使得两个进程都有各自的内存页面。这样一个进程所做的修改不会被另外一个进程可见了。请在 ucore 中实现这样的 COW 机制。

    由于 COW 实现比较复杂，容易引入 bug，请参考 <https://dirtycow.ninja/> 看看能否在 ucore 的 COW 实现中模拟这个错误和解决方案。需要有解释。

    请提供实现代码、测试用例和设计报告（包括在 COW 情况下的各种状态转换，类似有限状态自动机的说明）。

    这是一个 big challenge。

2.  说明该用户程序是何时被预先加载到内存中的？与我们常用操作系统的加载有何区别，原因是什么？
