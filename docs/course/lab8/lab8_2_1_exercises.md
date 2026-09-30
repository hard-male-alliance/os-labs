> 来源：[练习](http://8.135.34.58/lab2026/_book/lab8/lab8_2_1_exercises.html)

<span id="练习"></span>

### 练习

本实验围绕“文件系统分层读写 -\> 进程文件表与地址空间更新 -\> ELF 用户程序加载执行”三条相互衔接的主线展开。第一条主线从通用文件系统接口进入 SFS，最终通过块映射和磁盘块读写完成文件内容传输；第二条主线把文件描述符状态接入进程创建和进程切换，使进程能够继承文件访问能力并使用自己的页表；第三条主线从 `do_execve()` 打开的 ELF 文件中读取程序段，建立用户地址空间和用户栈，最后通过 `trapframe` 返回用户态。整体执行关系可以概括为 `kern_init -> fs_init -> proc_init -> user_main -> kernel_execve -> do_execve -> load_icode -> proc_run -> 用户态程序`，文件读取部分则贯穿 `sysfile_read -> file_read -> sfs_read -> sfs_io_nolock`。

本实验依赖实验2/3/4/5/6/7。请使用你此前设计的提示词生成相应实验代码，并将生成结果填写到本实验代码中标有“lab2”/“lab3”/“lab4”/“lab5”/“lab6”/“lab7”注释的对应位置，确保代码能够正确编译。由于实验八把文件系统接入了前面实验的进程和虚拟内存机制，补回前序代码后，还需要按照本实验的更新标记完成 `alloc_proc()`、`proc_run()` 和 `do_fork()` 的 Lab 8 更新。

<span id="功能模块一：进程文件表与地址空间的必要更新（设计提示词借助-ai-完成）"></span>

#### 功能模块一：进程文件表与地址空间的必要更新（设计提示词借助 ai 完成）

本功能模块不是重新设计进程管理，而是在补回实验四、实验五和实验六代码的基础上，为文件系统执行程序补充三个必要的更新点。代码中的 `lab8 function1-1`、`lab8 function1-2` 和 `lab8 function1-3` 分别对应下面三个任务。生成或补回代码时，不能删除已有的 `copy_files()`、`put_files()`、`copy_mm()` 等框架辅助函数。

<span id="任务一：更新进程控制块初始化"></span>

##### 任务一：更新进程控制块初始化

要求实现的函数有：

``` text
static struct proc_struct *alloc_proc(void); (kern/process/proc.c)
```

在补回前序实验对 `proc_struct` 各字段的初始化后，需要在 `//lab8 function1-1 YOUR CODE update alloc_proc` 位置完成 Lab 8 更新。`proc_struct` 新增的 `filesp` 保存当前进程的文件表指针，新进程尚未拥有文件表时必须将其初始化为 `NULL`。不能假定 `kmalloc()` 返回的内存已经清零，也不能把一个未初始化的文件表指针交给 `copy_files()`、`files_closeall()` 或 `put_files()` 使用。

整个任务的具体功能为：保证每个新建进程在进入文件系统相关路径前具有确定的文件表初始状态。内核线程和用户进程的文件表后续由创建流程分别建立或复制，`alloc_proc()` 只负责完成进程控制块的安全初始化。

整个任务的注意事项为：需要保留此前实验已经完成的字段初始化，并只补充 `proc->filesp = NULL` 这一 Lab 8 状态。不能在这里创建共享文件表，也不能提前调用 `files_create()`，因为 idle 进程和普通子进程的文件表建立时机不同。

整个任务可以借助的接口函数、结构体和数据结构为：`struct proc_struct`、`struct files_struct`、`kmalloc()` 以及 `NULL`。文件表的创建和引用计数由 `files_create()`、`files_count_inc()` 和 `copy_files()` 等后续流程负责。

<span id="任务二：更新进程切换中的地址空间处理"></span>

##### 任务二：更新进程切换中的地址空间处理

要求实现的函数有：

``` text
void proc_run(struct proc_struct *proc); (kern/process/proc.c)
```

在补回实验四的上下文切换代码后，需要在 `//lab8 function1-2 YOUR CODE update proc_run` 位置完成 Lab 8 更新。切换到目标进程前，应在关中断保护下更新 `current`，加载目标进程的页表根地址，刷新 TLB，再调用 `switch_to()` 完成内核上下文切换；切换完成后恢复中断状态。

整个任务的具体功能为：让调度器切换到用户进程时真正使用该进程由 `load_icode()` 或 `copy_mm()` 建立的地址空间。`load_icode()` 会为用户程序建立新的页目录，若 `proc_run()` 只切换寄存器上下文而不切换地址空间，进程将继续使用旧页表，用户代码、用户栈和内核映射都会出现错误。

整个任务的注意事项为：必须保持 `current`、目标进程的页表寄存器和 TLB 状态一致；页表切换和上下文切换期间不能被中断打断；不能把用户页表直接当作内核页表使用。应使用当前代码框架提供的 `local_intr_save()`、`local_intr_restore()`、`lsatp()`、`flush_tlb()` 和 `switch_to()`，并保留实验四要求的进程上下文切换语义。

整个任务的对外接口为：`proc_run()` 接收一个已经初始化并可运行的 `struct proc_struct`，使其成为当前运行进程；如果传入的进程已经是 `current`，不需要重复切换。调度器通过该接口完成进程切换，不需要了解页表切换的内部细节。

<span id="任务三：更新-fork-中的文件表复制与资源清理"></span>

##### 任务三：更新 fork 中的文件表复制与资源清理

要求实现的函数有：

``` text
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf); (kern/process/proc.c)
```

在补回实验四、实验五和实验六的进程创建流程后，需要在 `//lab8 function1-3 YOUR CODE update do_fork` 位置接入文件表处理。`do_fork()` 应在创建进程、分配内核栈、复制或共享地址空间的流程中正确调用 `copy_files()`，并在后续步骤失败时通过 `put_files()` 释放已经取得的文件表引用。

整个任务的具体功能为：使 `fork` 或 `clone` 创建的子进程拥有正确的文件访问状态。设置 `CLONE_FS` 时，子进程可以与父进程共享 `files_struct`；未设置时，应由 `copy_files()` 创建新的文件表并通过 `dup_files()` 复制打开文件状态。文件表复制成功后，进程的创建、加入进程链表、唤醒和返回子进程 PID 等前序实验流程仍必须完整执行。

整个任务的注意事项为：必须先补回前序实验代码，再将文件表复制接入正确的错误处理路径。`copy_files()` 失败时不能释放不存在的文件表；`copy_mm()` 或后续步骤失败时必须按相反顺序释放文件表、内核栈和 `proc_struct`；成功路径不能遗漏文件表引用计数。`copy_files()` 和 `put_files()` 本身是框架提供的辅助函数，本任务不要求重新实现它们。

整个任务的对外接口为：`do_fork()` 接收克隆标志、用户栈地址和父进程陷阱帧，成功时返回子进程 PID，失败时返回错误码；子进程必须拥有可用的进程控制块、内核栈、地址空间和文件表状态。

整个任务可以借助的接口函数、结构体和数据结构为：`alloc_proc()`、`setup_kstack()`、`copy_files()`、`copy_mm()`、`copy_thread()`、`hash_proc()`、`set_links()`、`get_pid()`、`wakeup_proc()`、`put_files()`、`put_kstack()`、`kfree()`、`struct files_struct`、`CLONE_FS` 和 `struct trapframe`。

<span id="功能模块二：sfs-文件读写功能（设计提示词借助-ai-完成）"></span>

#### 功能模块二：SFS 文件读写功能（设计提示词借助 ai 完成）

要求实现的函数有：

``` text
static int sfs_io_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, void *buf, off_t offset, size_t *alenp, bool write); (kern/fs/sfs/sfs_inode.c)
```

代码框架中的 `sfs_io_nolock()` 函数定义、局部变量、参数检查、`out:` 标签和文件大小更新逻辑已经全部移除，文件中只保留 `//lab8 function2 YOUR CODE`。请在该留白位置生成并填写完整的函数定义，而不是只填写首块、中间块或尾块的某一段代码。也就是说，生成结果必须自行包含函数签名、`din`、`endpos`、`blkoff`、读写函数指针、返回值、实际传输长度和错误清理所需的局部变量，以及原框架中的边界检查和 `out:` 收尾逻辑。

整个功能模块的具体功能为：`sfs_io_nolock()` 在文件偏移 `offset` 开始，把 `*alenp` 指定长度的数据在内存缓冲区和 SFS 磁盘块之间传输。`write` 为假时执行读操作，`write` 为真时执行写操作；两种操作共享块边界处理逻辑，但分别使用 `sfs_rbuf()`、`sfs_rblock()` 或 `sfs_wbuf()`、`sfs_wblock()`。实现需要依次处理首块非对齐区域、中间连续完整块和尾块非对齐区域，并将实际完成的字节数写回 `*alenp`。

整个功能模块的注意事项为：需要正确处理偏移为负、请求长度为零、超过 `SFS_MAX_FILE_SIZE`、读到文件尾以及读写区间跨越一个或多个块等情况。首块和尾块必须使用带块内偏移的缓冲区操作，中间完整块才能使用连续块操作；每一段操作前都要通过 `sfs_bmap_load_nolock()` 找到逻辑块对应的磁盘块。遇到磁盘读写或块映射错误时应立即返回，并保留已经成功传输的长度。写操作可能扩展文件大小，因此成功写入后需要正确更新 `sin->din->size` 和 `sin->dirty`；读操作不能把文件大小错误地扩大。

整个功能模块应该满足的对外接口为：`sfs_io_nolock()` 成功时返回零并通过 `*alenp` 返回实际读写长度，发生错误时返回相应错误码并保留已完成的传输长度。上层 `sfs_io()` 负责 inode 锁和 `iobuf` 位置更新，本函数不应重复获取 inode 锁，也不能绕过 `sfs_bmap_load_nolock()` 直接计算磁盘地址。

整个功能模块可以借助的接口函数、结构体和数据结构为：`struct sfs_fs`、`struct sfs_inode`、`struct sfs_disk_inode`、`sfs_bmap_load_nolock()`、`sfs_rbuf()`、`sfs_wbuf()`、`sfs_rblock()`、`sfs_wblock()`、`SFS_BLKSIZE`、`SFS_MAX_FILE_SIZE`、`E_INVAL` 以及 `sin->dirty`。`sfs_io()`、`sfs_read()` 和 `sfs_write()` 已经提供，不能删除或重新设计。

Tips：生成提示词时应明确要求 AI 同时支持读和写，覆盖“首块非对齐 -\> 中间完整块 -\> 尾块非对齐”的三段处理，并要求保留错误路径和实际长度更新。完成后应通过文件读写测试验证跨块读写、非对齐读写、EOF 读取和文件大小更新。

<span id="功能模块三：基于文件系统的用户程序加载与执行（设计提示词借助-ai-完成）"></span>

#### 功能模块三：基于文件系统的用户程序加载与执行（设计提示词借助 ai 完成）

要求实现的函数有：

``` text
static int load_icode(int fd, int argc, char **kargv); (kern/process/proc.c)
```

代码框架中的 `load_icode()` 函数定义和函数体已经全部移除，文件中只保留 `//lab8 function3 YOUR CODE`。请在该留白位置生成并填写完整的函数定义，必须使用上述固定函数签名；不能只补充 ELF 读取循环，也不能自行修改函数名、参数类型或返回值类型，因为 `do_execve()` 会直接调用该固定接口。

整个功能模块的具体功能为：`load_icode()` 负责把文件描述符 `fd` 对应的 ELF 用户程序加载到当前进程的新地址空间中。函数需要创建 `mm_struct` 和页目录，读取并检查 ELF 头，遍历程序头，为可加载段建立 VMA，分配并填充代码段和数据段页面，将 BSS 区域清零，建立用户栈，按 ABI 约定把 `argc` 和参数字符串写入用户栈，设置当前进程的 `mm`、页表寄存器和 `trapframe`，使进程从 ELF 入口地址返回用户态执行。

整个功能模块的注意事项为：必须区分 `p_filesz` 和 `p_memsz`，只从文件读取 `p_filesz` 字节，并把剩余的 BSS 区域清零；程序段可能不是页对齐的，需要正确处理首尾页面的偏移和长度。需要为每个程序段根据 `ELF_PF_R`、`ELF_PF_W` 和 `ELF_PF_X` 设置 VMA 标志和页表权限。用户栈必须映射在 `USTACKTOP - USTACKSIZE` 到 `USTACKTOP` 范围内，并保证写入参数时使用新地址空间中的有效页面。ELF 魔数错误、文件读取失败、内存分配失败或 VMA 创建失败时，都必须释放已经创建的 VMA、页表、物理页和 `mm_struct`，不能留下半初始化地址空间。

整个功能模块应该满足的对外接口为：`load_icode()` 接收已经打开的 ELF 文件描述符、参数数量和内核地址中的参数数组，成功时返回零并让当前进程具备返回用户态执行的完整现场，失败时返回错误码并清理临时资源。文件读取应通过 `load_icode_read()` 完成，该辅助函数负责定位文件偏移和读取指定长度，学生不需要删除或重新实现它。

整个功能模块可以借助的接口函数、结构体和数据结构为：`mm_create()`、`setup_pgdir()`、`load_icode_read()`、`mm_map()`、`pgdir_alloc_page()`、`mm_count_inc()`、`PADDR()`、`lsatp()`、`exit_mmap()`、`put_pgdir()`、`mm_destroy()`、`struct elfhdr`、`struct proghdr`、`ELF_MAGIC`、`ELF_PT_LOAD`、`ELF_PF_R`、`ELF_PF_W`、`ELF_PF_X`、`VM_READ`、`VM_WRITE`、`VM_EXEC`、`VM_STACK`、`USTACKTOP`、`USTACKSIZE`、`PTE_USER` 和 `struct trapframe`。

Tips：提示词应明确要求 AI 生成完整的 ELF 加载函数，包括成功路径、参数入栈、页表切换、陷阱帧设置和失败清理路径。完成后执行 `make qemu`，确认能够启动 `sh`，并在 Shell 中执行 `exit`、`hello` 以及其他位于 SFS 文件系统中的用户程序；还应检查 `fork` 后的子进程能否继续使用文件描述符。

对实验报告的要求：

- 基于 Markdown 格式完成报告，以文字分析为主。
- 提交功能模块一、功能模块二以及功能模块三每个任务所使用的完整 `prompt`。
- 说明功能模块一中 `alloc_proc()`、`proc_run()` 和 `do_fork()` 三个更新点与前序实验代码的关系，以及文件表、页表和 TLB 为什么必须保持一致。
- 分析 `sfs_io_nolock()` 如何处理首块、中间完整块和尾块，并说明读写共用实现的原因。
- 分析 `load_icode()` 如何从 ELF 文件建立 VMA、装载程序段、清零 BSS、建立用户栈并设置 `trapframe`。
- 展示 `make qemu` 的运行结果，说明是否成功进入 `sh`，并记录 `exit`、`hello` 等程序的执行情况。

<span id="扩展练习-challenge1：完成基于unix的pipe机制的设计方案"></span>

#### 扩展练习 Challenge1：完成基于“UNIX的PIPE机制”的设计方案

如果要在ucore里加入UNIX的管道（Pipe）机制，至少需要定义哪些数据结构和接口？（接口给出语义即可，不必具体实现。数据结构的设计应当给出一个（或多个）具体的C语言struct定义。在网络上查找相关的Linux资料和实现，请在实验报告中给出设计实现”UNIX的PIPE机制“的概要设方案，你的设计应当体现出对可能出现的同步互斥问题的处理。）

<span id="扩展练习-challenge2：完成基于unix的软连接和硬连接机制的设计方案"></span>

#### 扩展练习 Challenge2：完成基于“UNIX的软连接和硬连接机制”的设计方案

如果要在ucore里加入UNIX的软连接和硬连接机制，至少需要定义哪些数据结构和接口？（接口给出语义即可，不必具体实现。数据结构的设计应当给出一个（或多个）具体的C语言struct定义。在网络上查找相关的Linux资料和实现，请在实验报告中给出设计实现”UNIX的软连接和硬连接机制“的概要设方案，你的设计应当体现出对可能出现的同步互斥问题的处理。）
