# Lab 5：用户进程、系统调用与写时复制

| 项目 | 内容 |
|---|---|
| 小组成员 | 龙李宽-2413077、王艺霖-2410824、郭嘉睿-2413174 |
| 完成日期 | 2026-11-21 |
| 运行环境 | Debian trixie 容器，QEMU 10.0.13 `virt`，RV64，单 hart，128 MiB |
| 工具链 | riscv64-unknown-elf GCC 14.2.0、GDB 16.3、OpenSBI 1.6 |
| AI 协作 | Codex Desktop；本轮整体委派给同一个 agent，没有按成员虚构独立的提示词或实现记录。实际任务与分模块规格见[提示词文档](prompt.md)。 |

## 一、目标与整体逻辑

Lab 4 已经能保存和恢复内核线程，Lab 5 把用户地址空间、CPU 特权级和进程生命周期连成一条可运行的路径：

```text
idle → init → user_main → exec 装载 ELF → sret → 第一个 U 态程序
           ↑                                    │
           │               ecall → 分发 → fork / yield / kill / exec
           └── 唤醒父进程 ← exit（僵尸）←───────┘
               wait 写回状态、回收 PCB 和内核栈
```

先建立系统调用接口，再让第一个程序获得一套可以被 `sret` 恢复的寄存器和地址空间；
有了可运行的用户进程，才能验证复制、调度和退出。退出的验证也反过来检查前面各阶段是否正确移交了资源所有权。

## 二、核心实现与规格

| 功能模块 | 关键函数与约束 |
|---|---|
| 1. 系统调用 | `SAVE_ALL` / `RESTORE_ALL`、`exception_handler`、`syscall`。进入内核后 `sscratch=0`，嵌套 S 态陷阱留在当前栈；回 U 态前在 `sscratch` 安装内核栈顶。uCore ABI 是 `a0` 存调用号、`a1–a5` 传参、`a0` 返回结果。只有 U 态 `ecall` 提前推进 `sepc` 四字节。 |
| 2. 第一个用户进程 | `proc_init`、`init_main`、`kernel_thread`、`kernel_execve`、`do_execve`、`load_icode`。加载器校验 ELF（Executable and Linkable Format），复制 `PT_LOAD` 文件字节并清零剩余空间；地址空间和入口完全就绪后才切页表并改陷阱帧。装载失败的 U 态 `exec` 可返回旧程序。 |
| 3. 复制 | `do_fork`、`copy_mm`、`dup_mmap`、`copy_range`。先构造 PCB、内核栈、子页表与返回现场，再在关中断期间登记 PID、接入进程树并标为可运行。子进程的返回寄存器置 0，父进程取得 PID；普通 `fork` 通过写时复制实现两者的独立写入。 |
| 4. 控制与结束 | `do_exit`、`do_wait`、`do_yield`、`do_kill`。退出先切离并释放地址空间，再记录退出码、唤醒等待者，并把孤儿交给 init。父进程成功写回退出码后才摘除子进程、释放内核栈与 PCB。时钟中断请求重新调度，`kill` 可唤醒因等待子进程而睡眠的任务。 |

### 内核栈和地址空间的所有权

U 态的陷阱帧（trap frame）固定在任务内核栈顶；首次启动的内核线程从该帧下方开始使用栈，
给之后的 `kernel_execve` 留出同一个位置。没有必要用 S 态 `ebreak` 冒充系统调用：
内核初始化用户帧，然后直接接入公用 `forkrets → __trapret` 路径。
返回 S 态或 U 态取决于保存的 `sstatus.SPP`，而不是当前 C 函数或当前栈地址。
硬件寄存器行为见 [RISC-V 特权架构手册](https://github.com/riscv/riscv-isa-manual/blob/main/src/priv/supervisor.adoc)。

内核高半区映射由所有根页表借用；每个用户地址空间只拥有低半区用户页表树和其中的映射引用。
`exit` 或失败的装载在释放前先确保该页表不再活动。销毁只遍历自己拥有的低地址页表项，
不依赖虚拟内存区域（virtual memory area, VMA）的顺序或完整性，因此也能收拾部分构建的页表。
`wait` 不释放当前正在用的栈：它回收的是已经离开 CPU 的子进程的栈。

## 三、扩展练习

### 1. 写时复制（Copy-on-Write, COW）

| 映射状态 | 触发事件 | 后继状态 |
|---|---|---|
| 私有可写 `W=1` | 普通 `fork` | 父子都为 `W=0,COW=1`，物理页引用加一；返回前刷新父页表的地址翻译 |
| COW，引用数 > 1 | 用户写故障，或内核向用户写回 | 给写者复制一页并恢复 `W`，旧页少一个映射引用；其他映射不变 |
| COW，引用数 = 1 | 用户或内核首次写 | 原页直接升级为可写，不复制内容 |
| 真正只读，`COW=0` | 尝试写入 | 不升级权限；用户异常按非法写处理 |
| 任意状态 | 退出或替换映像 | 释放映射引用；引用归零才归还物理页 |

与 [xv6 对页故障和 COW 的讨论](https://mit-pdos.github.io/xv6-riscv-book/pgfault.html) 一样，
把复制推迟到实际写入：没有被写过的栈页和数据页可以共享，代码页一直只读。
内核的 `copy_to_user` 和用户写故障走同一套 COW 拆分逻辑；VMA 的逻辑写权限不能替代
对当前页表项的检查。分配失败时，已有映射保持原样；若 `fork` 中途失败，父进程可能留下
引用为 1 的 COW 页，下一次写原地恢复。这不是多核并发页管理实验：实现依赖单 hart，
不包含 Linux 的并发页固定和文件回写机制，也没有复现历史漏洞。

专项程序 `cow` 让一个子进程修改数据、另一个子进程持续检查原值；
父进程等待第一个子进程时，由内核把退出码写入仍由第二个子进程共享的不同页。
两个视角都验证写入没有穿透到其他地址空间。

### 2. 用户程序何时被预先加载

`code/Makefile` 先把各个用户程序与用户库链接成独立 ELF，再用 `ld --format=binary`
把 ELF **文件字节**链接成内核的数据。OpenSBI 启动 QEMU 的内核镜像后，这些字节已经在 RAM，
但仍不是任何进程的用户映射。到 `user_main → do_execve`，加载器才建立用户页表、将 ELF
段复制到 `p_va` 对应的页面，并用 `e_entry` 作为返回地址。

因此有两个时间点：载入内核镜像时拿到 **ELF 字节**，执行 `exec` 时才获得 **可执行的用户映像**。
这个做法让启动不依赖尚未实现的文件系统。Linux 则由
[`fs/binfmt_elf.c`](https://github.com/torvalds/linux/blob/master/fs/binfmt_elf.c)
从文件建立相应映射，通常按访问调页，并支持动态解释器、参数栈和其他 ELF 布局。
Lab 5 的加载器面向本课程静态、分页后区间互不重叠的 ELF，装载时直接分配并填好页面。

## 四、双层 GDB：一次 `SYS_putc('H')`

| 观测点 | 实测状态 |
|---|---|
| 来宾 `ecall` 前 | `pc=0x8000a2`，`a0=30`，`a1=72` |
| 宿主异常助手 | QEMU `helper_raise_exception` 从动态生成代码进入；`priv=0`、异常号 8，随后到 `riscv_cpu_do_interrupt` |
| 来宾陷阱入口 | `pc=__alltraps`，`scause=8`，`sepc=0x8000a2`，`SPP=0` |
| 来宾 `sret` 前 | `sepc=0x8000a6`，`a0=0`，`sscratch` 为当前任务内核栈顶 |
| 宿主返回助手 | QEMU `helper_sret` 在 `priv=1` 时读取 `sepc=0x8000a6`，为下一段执行恢复 U 态 |
| 来宾返回 | `pc=0x8000a6`，`a0=0`，停在 `ecall` 的下一条指令 |

在 [QEMU 的指令翻译入口](https://github.com/qemu/qemu/blob/v10.0.13/target/riscv/insn_trans/trans_privileged.c.inc)
中，`trans_ecall` 生成异常助手调用；实际执行时才退出当前翻译块（translation block, TB），
并由 [`riscv_cpu_do_interrupt`](https://github.com/qemu/qemu/blob/v10.0.13/target/riscv/cpu_helper.c)
根据当时的特权级和委托寄存器填写异常状态。相似地，`trans_sret` 发出
[`helper_sret`](https://github.com/qemu/qemu/blob/v10.0.13/target/riscv/op_helper.c)
调用并终止当前 TB，避免沿用基于原特权级的翻译上下文。
[TCG（Tiny Code Generator）文档](https://www.qemu.org/docs/master/devel/tcg.html)解释了 TB 为什么依赖 CPU 状态。
Lab 2 的页表走查也由来宾 MMU 状态触发，但通常发生在访存的地址翻译慢路径；
这里是特权指令本身在翻译时显式发出助手调用。

调试从内核符号加上 `add-symbol-file obj/__user_hello.out` 开始；
从 `objdump -d` 找到 `sys_putc` 里真正的指令地址再下硬件断点。
在来宾停于 `ecall` 前附加第二个 GDB 到 QEMU 进程，此时再设置宿主断点以排除 OpenSBI 启动期间的异常。
第一次只用 `stepi` 时，来宾 GDB 已经越过陷阱，很容易误把后续时钟中断的 `sret`
当成目标返回；改用 `scause == 8` 和对应 `sepc` 的临时断点后，两边的地址一致。
复现细节与调试日志见[双层调试说明](evidence/debug.md)。

## 五、整机验证与迭代

```sh
docker build -t os-labs:lab5 -f docker/Dockerfile docker
docker run --init --rm --user "$(id -u):$(id -g)" \
  -v "$PWD:/workspace" os-labs:lab5 make grade
docker run --init --rm -it --user "$(id -u):$(id -g)" \
  -v "$PWD:/workspace" os-labs:lab5 make qemu
```

实际复用了按同一 Dockerfile 构建的 `os-labs:lab3` 镜像。普通启动在 init 空闲时
持续运行，可用 `Ctrl-a x` 结束。截图为实际终端输出回放。

- [普通启动日志](evidence/boot.log)：`hello` 报告 PID 2，退出码为 0，init 确认内存基线恢复。
- [课程评分日志](evidence/grade.log)：**130/130**，13 个用户程序，包括 32 子进程批量创建、
  仅靠时钟抢占的 `spin`、错误指针和用户异常；注入测试断言与计分条件保持原样。
- 额外串口记录：[COW](evidence/cow.log)、[用户态 exec](evidence/exec.log)、
  [forktree](evidence/forktree.log)、[waitkill](evidence/waitkill.log)。
  `exec` 先尝试截断映像，确认失败返回旧程序，再加载有效映像并由父进程观察到新的退出码 37。

![QEMU 启动](images/boot.png)

![Lab 5 课程评分](images/grade.png)

| 实际迭代发现 | 处理 |
|---|---|
| 首次编译发现 Lab 5 仍缺 `switch_to`；GCC 14 默认汇编 ISA 未开启 `Zifencei`。 | 移植 Lab 4 的切换汇编；只在安装新用户指令需要 `fence.i` 的位置声明该扩展。 |
| `divzero` 先失败于输出不符。 | 原测试的 C 除零是未定义行为，优化器不保证发出除法。改用 RISC-V `divw` 测 ISA 结果，评分期望保持不变。 |
| `forktest` 跑完 32 个子进程却过不了内存检查。 | SLOB 内核堆之前一直持有扩容页，且 `kallocated()` 恒返回零。改为页内合并、整页归还，并统计仍在用的内核堆字节。最终保留并通过原本的物理页及堆内存双重检查。 |

## 六、原理与收获

- **硬件保存的特权状态不是完整上下文。** CPU 写 `sepc/scause/sstatus`，内核还要安排内核栈并保存通用寄存器。调度切换只用调用约定要求保留的寄存器。
- **退出和回收是两件事。** 进程可以放下地址空间并不再调度，但退出状态和内核栈仍有其他所有者；`wait` 是生命周期的最后一步。
- **故障也可以是普通控制流。** COW 的写故障不是程序错误；真正只读和「暂缓写入」必须有可区别的状态，内核写回和用户写不能各自实现一套判断。
- **本章未覆盖**多核同步及跨核 TLB 失效、通用按需调页与交换、VFS/页缓存、动态链接、完整信号与权限模型。
- **AI 协作经验：** 在提示里写清谁拥有资源、何时发布状态、谁负责回收，比要求“补全几个函数”更能约束跨模块错误。以系统级内存基线和寄存器实测检验这些约束，而不是只看打印了几行成功信息。
