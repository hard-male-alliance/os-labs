# Lab3 提示词与规格记录

## 用户原始需求

> 哦哦，好的。接下来你去实现 lab3 吧。需要的时候自己开 docker 用。

以下工作提示词与规格由助手根据仓库中的课程要求和源码，在实现前整理形成，
并非用户另外发送的消息。课程练习文档已记录教师取消用户态／内核态切换拓展任务
（Challenge）的调整。本文件保留课程规定的四段式标签，正文使用中文。

## 功能模块一：陷阱入口与上下文恢复

[PROMPT]
在 `code/kern/trap/trapentry.S` 中实现 `RESTORE_ALL` 和 `__alltraps`。
保留 `SAVE_ALL`、`struct trapframe`、公开函数签名以及原有的 `lab3` 标记。

[RELY]
`SAVE_ALL` 在当前内核栈上分配 `36 × REGBYTES` 字节，形成陷阱帧（trap frame）。
槽位 0—31 对应 `x0`—`x31`；其中槽位 2 保存通过 `sscratch` 读取的、陷阱发生前的
`sp`。槽位 32—35 依次保存 `sstatus`、`sepc`、`stval`、`scause`。
根据 RISC-V 调用约定，C 函数的第一个参数通过 `a0` 传递。
本实验在现有的监督态内核栈上处理陷阱。

[GUARANTEE]
完成 `RESTORE_ALL` 和 `__alltraps`，并通过 `__trapret` 后的 `sret` 返回。

[SPECIFICATION]
`__alltraps`：

- 前置条件（Pre-Condition）：硬件已进入监督态陷阱入口，当前使用的内核栈按
  16 字节对齐；陷阱相关控制与状态寄存器（Control and Status Register, CSR）
  记录了被打断时的状态。
- 后置条件（Post-Condition）：`SAVE_ALL` 建立陷阱帧，C 处理函数收到的第一个
  参数恰好是该帧的地址；C 函数返回后进入 `__trapret`，此时 `sp` 仍指向该帧。

`RESTORE_ALL`：

- 前置条件（Pre-Condition）：`sp` 指向由 `SAVE_ALL` 建立的完整 36 槽陷阱帧，
  帧中包含 C 处理函数对返回状态所作的修改。
- 后置条件（Post-Condition）：采用帧中的 `epc`、`status` 返回状态；在随后执行
  `sret` 之前，恢复全部可写整数寄存器以及原始 `sp`。

约束要求（Requirements）：

完成对陷阱帧的全部访问后，最后恢复 `sp`。先恢复 `status`、`epc`，再恢复用作
临时存储的寄存器。`stval`、`scause` 是诊断信息，不属于必须写回的返回控制状态。
不引入已取消的用户态／内核态切换拓展任务。

## 功能模块二：中断与异常处理

[PROMPT]
在 `code/kern/trap/trap.c` 中实现 `interrupt_handler` 和 `exception_handler`，
使用框架提供的时钟、诊断输出及 SBI 关机接口。

[RELY]
可依赖 `struct trapframe`、`riscv.h` 中的 `IRQ_*` 和 `CAUSE_*`、`ticks`、
`clock_set_next_event`、`print_ticks`、`print_trapframe`、`sbi_shutdown`。
本实验使用的 RV64GC 指令包含 16 位和 32 位两种长度。
骨架定义了 `TICK_NUM=100`，但缺少指导书提到的 `SHUTDOWN_AFTER`。

[GUARANTEE]
完成以下函数：

```c
void interrupt_handler(struct trapframe *tf);
void exception_handler(struct trapframe *tf);
```

补充 `SHUTDOWN_AFTER=10`，即输出十次统计信息、累计 1000 次时钟中断后关机，
并包含 `sbi.h`。保持 `trap.c` 中已有函数及其签名不变。

[SPECIFICATION]
`interrupt_handler`：

- 前置条件（Pre-Condition）：`tf` 指向描述当前中断的有效陷阱帧。对于监督态
  时钟中断，时钟初始化已建立定时事件并将 `ticks` 清零；陷阱入口已关闭监督态中断。
- 后置条件（Post-Condition）：根据原因码分类处理，同时保留帧中的返回状态。
  监督态时钟中断安排下一次事件，并且只将 `ticks` 增加一次；每到达一个完整的
  统计周期，输出规定的信息，累计输出达到配置次数后调用 SBI 关机。
  其他已识别的中断输出类别；未知中断输出原因码和完整陷阱帧。

`exception_handler`：

- 前置条件（Pre-Condition）：`tf` 指向有效异常帧。对于非法指令和断点异常，
  `epc` 指向当前内核代码映射中可读取的、触发异常的 16 位或 32 位指令。
- 后置条件（Post-Condition）：非法指令和断点异常输出规定的诊断信息，并将
  `epc` 精确前移一条触发指令的长度。其他异常只输出完整帧，不修改 `epc`。
  帧中的其余字段保持不变，以便统一返回路径使用。

约束要求（Requirements）：

时钟中断必须安排下一次事件，每次只增加一次 `ticks`，在 `TICK_NUM` 的每个
整数倍处输出统计信息，并在累计输出达到 `SHUTDOWN_AFTER` 后关机。
已识别的非时钟中断输出类别，未知原因输出原因码和完整帧。
非法指令及断点异常输出异常类型和触发地址，根据指令低两位 `[1:0]` 将 `epc`
增加 2 或 4，使处理后能够继续执行。其他异常输出陷阱帧，不虚构恢复行为。

## 实验集成与交付

[PROMPT]
复用 `origin/lab2` 中已完成的 `page_init` 和最佳适应（Best-Fit）分配器，
不重新设计 Lab2。保留 Lab3 在内存分配接口中的中断保护，以及课程现有测试。
使用 Docker 中的 RISC-V 工具链构建，并在真实 QEMU 来宾机中启动内核。
检查压缩与非压缩指令的异常返回、寄存器恢复、持续时钟中断、统计输出周期以及
实际关机行为。保存实验报告和验证证据，不增加已取消的拓展任务。

[RELY]
可依赖 `origin/lab2` 的 `8f7daa9` 提交、仓库中的课程评分脚本、测试注入器及
上下文测试，以及 RISC-V 特权／非特权指令集规范、QEMU `virt` 启动文档和
xv6 的陷阱处理资料。

[GUARANTEE]
交付可正常启动的内核和可运行的课程评分流程、有针对性的来宾机补充检查、
可复现的容器工作流程、`report/report.md`，以及真实终端输出证据。

[SPECIFICATION]

- 前置条件（Pre-Condition）：已有未注入测试的 Lab3 骨架、已完成的 Lab2
  依赖源码、课程测试，以及正常运行的 Docker 守护进程。
- 后置条件（Post-Condition）：提交内核能够构建，并完成从正常启动到时钟驱动
  关机的全过程；隔离的评分内核和探针内核覆盖要求的陷阱行为；报告、提示词、
  截图和保留日志描述实际观察到的结果。提交内核的启动路径不包含测试注入代码。

约束要求（Requirements）：

保留公开入口和课程测试语义。对于真实执行暴露的工具链或启动兼容问题，修复
对应的集成环节。不隐藏失败输出，不宣称执行过实际未运行的测试。
运行后记录观察到的问题及修复方法。

## 独立评审后的规格完善

独立评审区分了入口与恢复各自的前置条件，并为两个处理函数及集成流程补充了
明确的前置／后置条件，使其符合课程提示词模板。这些修改澄清了已有行为，
没有因此修改内核代码。
