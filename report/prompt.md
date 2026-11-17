# Lab4 提示词与规格

本次任务是在 `lab4` 分支完成实验，在 Docker 中运行验证，并用中文提交报告与提示词。
以下是根据课程练习、仓库代码和验证反馈整理的可复用工作提示词；它们不是额外的用户消息。

## 一、虚拟内存页操作

[PROMPT]

在 `kern/mm/pmm.c` 实现 `page_insert` 和 `page_remove`，保持公开签名。
复用 Lab2 的物理内存初始化和最佳适配分配器（best fit），适配本分支的管理器名称。

[RELY]

页表是 Sv39，实验动态映射以 4 KiB 为单位；启动代码另建立一个 1 GiB 叶项。
使用 `get_pte` 定位槽位，使用 `pte_create` 编码页号和权限，
使用 `pte2page`、`page_ref_inc`、`page_ref_dec` 和 `free_page` 维持映射引用。
单核下用 `sfence.vma` 同步地址翻译。

[GUARANTEE]

成功映射后，叶项权限取调用者的 `perm`，同页更新不改变引用数；
跨页替换后，旧页引用与其剩余映射数一致。删除空映射不分配、不释放。
若中间页表分配失败，返回 `-E_NO_MEM`，不留下半条新路径。

[SPECIFICATION]

1. 定位或创建槽位后，记录旧页；新旧页不同才取新引用。
2. 写入新页表项，再同步该地址翻译；旧页引用清零后方可交还分配器。
3. 删除时以只查找模式取得槽位；清项、同步、放引用的顺序不能颠倒。
4. 表结构变更用全量本核栅栏；逐页修改用按虚拟地址的栅栏。
   新建根项下两层时，先分配并连好下级，再挂到根项；失败则释放未发布的页。
   非叶项不设置 `U/A/D`，不把高层叶项解析成下一层页表。
5. 用启动自检和课程 PMM 测试检查物理页别名、权限覆盖、重复映射与重复删除。

## 二、进程创建与初始化

[PROMPT]

实现 `alloc_proc`、`proc_init`、`kernel_thread`、`do_fork`、`copy_thread`、`copy_mm`。
不引入用户进程、地址空间复制或退出回收；保留课程入口和函数签名。

[RELY]

`proc_list` 管理非 idle 线程，PID 哈希表用于检索；PID 0 留给 `idleproc`。
`boot_pgdir_pa` 是页表根的物理地址。`kernel_thread_entry` 从 `s0/s1` 取得函数和参数。
Lab3 的 `RESTORE_ALL` 按 36 个 RV64 槽恢复，最后从陷阱帧加载 `sp`。

[GUARANTEE]

`proc_init` 后当前是 PID 0 的 `idleproc`，复用启动栈；PID 1 的 `initproc` 已登记且可运行。
新线程有独立内核栈，沿 `forkret → forkrets → __trapret → sret` 到达入口。
创建失败不发布 PCB（process control block，进程控制块），且归还已分配对象。

[SPECIFICATION]

1. 初始化 PCB 为未运行、PID -1、共享启动页表；其余字段清零。
2. 先分配内核栈并组织子线程陷阱帧。初次 `sret` 返回 S 态：
   `SPP=1, SPIE=1, SIE=0`。调度上下文的 SP 指向陷阱帧，恢复后的 SP 指向内核栈顶
   （或 `do_fork` 收到的非零栈地址）。
3. 在同一关中断区间分配 PID，插入哈希表和队列、计数，再标记可运行。
4. 确认 Lab3 恢复路径服务于真实中断和新线程首次运行，但不把 Lab3 的定时关机策略移入 Lab4。

## 三、调度与上下文切换

[PROMPT]

实现 `cpu_idle`、`schedule`、`proc_run` 和 RV64 的 `switch_to`。
使用课程现有单核模型和循环链表，保留线程入口和测试注入点。

[RELY]

`struct context` 顺序为 `ra, sp, s0…s11`，每项宽度 `REGBYTES`。
`proc->pgdir` 保存物理字节地址；不能用面向 Sv32 的 `lsatp`。
中断保存与恢复使用 `local_intr_save/restore`。

[GUARANTEE]

从当前线程之后循环选择第一个可运行线程；扫描无结果退回 idle。
第一次调度能启动 `initproc`；已调度过的线程再次被选中时能从先前的 `schedule` 调用续跑。

[SPECIFICATION]

1. 调度时清当前线程的 `need_resched`，跳过队列哨兵，选中线程的 `runs` 加一。
2. 同线程不切换；否则在关中断区间更新 `current`，页表根不同时按
   `(8UL << 60) | (pgdir >> 12)` 写入 `satp` 并执行 `sfence.vma`。
3. 汇编先完整保存旧上下文，再加载新上下文并 `ret`。
   首次运行跳到 `forkret`；恢复运行跳回自己的 C 调用现场，再恢复先前的中断状态。
4. 以 `make qemu` 验证普通启动，以 `make grade` 验证两个新线程反复让出 CPU；
   不把框架仍未实现的 `do_exit` 当作本次调度失败。

## 集成反馈提示词

- 若 OpenSBI 的交接地址为零，先对比 Lab3 的 `-kernel` 启动路径；
  切到高半区前保留物理 PC 的临时恒等映射，跳转后再撤销。
- 若页表结构断言通过、实际低地址读写却不继续，先检查**硬件**页表遍历规则，
  尤其是非叶项的保留位，不把软件检查能读到的项误当成硬件认可的映射。
- 评分测试依然用官方注入流程与评分标准；若 GCC 报回调不匹配，只把测试线程
  回调签名对齐到 `int (*)(void *)`，不改断言和评分项。
