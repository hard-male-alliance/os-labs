# Lab 1 提示词汇总

## 记录范围

本文件按任务开展顺序整理课程文档获取、开发环境配置、项目管理及 Lab 1 练习相关的提示词。第 1–7 项根据实际任务指令作规范化表述，保留原有任务目标与范围，不作为逐字对话记录。

记录范围包括实验准备阶段的具体任务，以及第 8 项中两道练习的分步交互流程；不包含代理内部配置、系统提示词及完整助手回复。各项工作说明用于交代任务范围、处理结果与实施状态。Lab 1 的两道练习以代码理解和运行调试为主，相关记录按实际任务性质组织。

## 1. 获取并整理课程指导书

```text
请获取 http://8.135.34.58/lab2026/_book/ 下的课程指导书内容，
按照原有章节结构整理为 Markdown 文档，并保存到项目 docs/ 目录。
请保留来源链接、代码示例和相关图片资源，便于后续阅读与核对。
```

工作说明：课程正文整理至 `docs/course/`，保留章节结构、来源链接和图片资源。

## 2. 分析课程指导书的内容与技术说明

```text
请阅读课程指导书的代表性章节，从内容组织、知识衔接、
技术表述和实验操作说明等方面进行分析，并指出需要进一步核对的内容。
```

工作说明：阅读代表性章节并核对部分技术说明。该任务随后暂停，未完成全书评审。

## 3. 配置实验开发环境与 CLion 项目

```text
请暂停指导书评审，转为检查并配置现有实验项目。
核对交叉编译工具链、构建入口和调试条件，配置 CLion 项目，
使其能够识别实际 Makefile、编译参数及源码目录，并支持运行和远程调试。
可通过可用的 CLion 集成接口及 .idea 配置完成相关设置。
如需安装依赖或执行需要 sudo 权限的操作，请列出准确命令，由我执行。
```

工作说明：检查交叉工具链，配置 Make 构建入口，生成编译数据库，并配置 CLion 运行和远程调试。系统依赖由用户安装。

## 4. 确认 CLion 的 Makefile 项目重新加载方式

```text
请协助定位 CLion 中 Reload Makefile Project 的入口，
说明如何在修改构建配置后重新加载项目，使编辑器更新编译信息。
```

工作说明：该请求后紧接着进入构建系统评估，未形成独立完成的排查记录。

## 5. 评估 Make 与 CMake 构建方案

```text
请基于现有根目录 Makefile 和课程构建流程，评估迁移至 CMake 的必要性。
比较两种方案在交叉编译、编辑器集成、调试配置、维护成本及课程兼容性方面的差异。
如需验证方案，请使用隔离原型，先提供评估结果，再决定是否调整正式构建系统。
```

工作说明：比较构建方案，并在项目的 `.temp/`、`.cache/` 下验证隔离原型。正式构建系统保持 Make；下一项任务明确决定不迁移。

## 6. 保留 Make 构建并配置锁定的 Python 环境

```text
请保留现有 Makefile 构建方式，不迁移至 CMake。
使用 uv 管理项目辅助脚本所需的 Python 环境，建立项目配置和依赖锁文件，
并统一脚本运行入口，使依赖安装和后续执行能够按锁定版本复现。
```

工作说明：建立 `pyproject.toml`、`uv.lock`、`.python-version` 与本地虚拟环境，将公共 Python 工具入口接入 `uv run --locked python`。

## 7. 整理交付目录与 Git 分支基线

```text
请按照 docs/ 中的交付规范整理目录及 Git 分支结构，并完成相应提交。
将 uv 配置、课程文档、Git 约定和 clang-format 等公共内容保留在 main 分支，
建立 milestone 标签作为后续实验的公共基线。
从该基线创建 lab1 分支，将本实验源码放入 code/，
将报告、提示词和测试截图放入 report/，便于按实验分别交付。
```

工作说明：公共配置提交至 `main`，创建带注释的 `milestone/common-base` 标签，并从该标签创建 `lab1` 分支。实验源码位于 `code/`，报告、提示词和截图位于 `report/`。

## 8. 完整完成 Lab 1

### Lab1 练习一

#### 步骤 1：明确练习一的任务与证据

##### 我的提示词

```text
[PROMPT]
任务：我负责 Lab1 练习一。请阅读练习原文和相关源码，
明确需要解释的入口操作，以及完成分析需要哪些源码和证据。
操作要求：先只读检查当前目录、分支、工作区和相关文件。
输出要求：给出“问题—相关文件—观察方法—完成证据”对应表，
本轮只明确任务，不提前给出完整练习答案。

[RELY]
仓库：/home/wyl/os_work_lab/os-labs，实验分支 lab1。
练习要求：docs/course/lab1/lab1_2_1_exercise.md。
相关源码：code/kern/init/entry.S、init.c，
code/kern/mm/mmu.h、memlayout.h，code/tools/kernel.ld。
练习一要求说明 la sp, bootstacktop 和 tail kern_init
分别完成什么操作、目的是什么。

[GUARANTEE]
交付练习一的两个核心问题及对应证据清单。
说明相关文件各自提供什么信息。
明确这是理解与验证题，没有新增内核函数的要求。

[SPECIFICATION]
Pre-Condition：定位当前仓库并确认 lab1 分支，检查已有修改。
Post-Condition：我能说明本练习要回答什么，以及如何验证答案。
Case 1：材料齐全，列出阅读顺序。
Case 2：材料缺失，说明缺少什么及其影响。
Requirements：区分教师必答内容和用于支撑答案的补充观察。
```

##### Agent 输出概要

确认任务范围，列出入口汇编、栈宏、链接脚本和 C 入口之间的关系，确定后续需要源码、符号、反汇编和寄存器记录。

#### 步骤 2：阅读入口汇编与 C 入口

##### 我的提示词

```text
[PROMPT]
任务：逐行解释 entry.S，并结合 init.c 理解为什么先设置栈，
再进入 kern_init。
操作要求：读取真实源码，用初学者能够理解的语言解释。
本轮进行源码分析，不执行 GDB，也不假定伪指令的最终展开。
输出要求：区分预处理指令、汇编器指令、标签和 CPU 指令，
给出 entry.S 到 kern_init 的执行顺序。

[RELY]
code/kern/init/entry.S：
kern_entry:
    la sp, bootstacktop
    tail kern_init

该文件还包含 #include、.section、.globl、.align、.space，
以及 bootstack、bootstacktop 标签。
code/kern/init/init.c 声明 kern_init 为 noreturn，
函数末尾进入无限循环。

[GUARANTEE]
解释大写 .S、头文件引用、代码节、数据节和全局符号。
解释标签代表地址，以及 .space 如何预留空间。
初步说明 la 的地址加载语义、tail 的跳转语义，
以及 kern_init 不返回与入口设计的关系。

[SPECIFICATION]
Pre-Condition：已读取 entry.S 和 init.c 的当前内容。
Post-Condition：我能说明入口代码为何先建立内核栈再进入 C。
Case 1：遇到宏定义，定位其来源，留到下一步计算。
Case 2：遇到伪指令，说明其语义，将具体展开留到反汇编确认。
Requirements：kern_entry 是汇编入口标签，不虚构普通 C 函数签名；
noreturn 是编译器契约，实际不返回由代码行为保证。
```

##### Agent 输出概要

解释 `entry.S` 的结构与两条关键伪指令，说明内核需要自己的栈，以及 `kern_init` 为什么不需要返回汇编入口。

#### 步骤 3：分析启动栈与链接布局

##### 我的提示词

```text
[PROMPT]
任务：分析 bootstack、bootstacktop 的含义，计算启动栈大小，
并理解链接脚本如何组织入口代码和栈空间。
操作要求：读取 mmu.h、memlayout.h 和 kernel.ld，
所有宏和值均从当前源码取得。
输出要求：给出宏定义表、大小计算和栈布局示意图；
此阶段没有读取 ELF 时，不填写实际链接地址。

[RELY]
相关宏：PGSIZE、PGSHIFT、KSTACKPAGE、KSTACKSIZE。
entry.S 在 .data 中通过 .align PGSHIFT 对齐，
在 bootstack 后使用 .space KSTACKSIZE，随后定义 bootstacktop。
kernel.ld 包含 ENTRY(kern_entry)、BASE_ADDRESS 和各节布局。

[GUARANTEE]
解释 KSTACKSIZE = KSTACKPAGE × PGSIZE：
KSTACKSIZE 为栈总字节数，KSTACKPAGE 为栈页数，
PGSIZE 为每页字节数。
解释半开区间 [bootstack, bootstacktop)、
向低地址增长和初始 SP 选择 bootstacktop 的原因。
解释页对齐与 ABI 栈对齐要求，并说明栈位于 BSS 清零范围之外。
说明 ELF 入口声明、代码实际排列与镜像装载的区别。

[SPECIFICATION]
Pre-Condition：相关宏和链接脚本已读取。
Post-Condition：我能计算栈大小，并解释栈底、栈顶及初始 SP。
Case 1：bootstacktop 没有额外分配空间，解释它是区间末端标签。
Case 2：链接脚本声明入口，但代码位置还依赖节排列，
说明后续需要检查最终 ELF。
Requirements：页大小宏不能作为已经实现分页的证据；
栈增长方向来自调用约定，而非这两个标签自行决定。
```

##### Agent 输出概要

从当前宏计算栈大小，解释栈向下增长、栈顶是区间末端地址，以及 `.data`、对齐和链接脚本的作用。

#### 步骤 4：构建并核对实际符号与反汇编

##### 我的提示词

```text
[PROMPT]
任务：使用现有 Makefile 构建内核，取得入口与栈符号地址，
核对 la、tail 的实际机器指令。
操作要求：先确认 Makefile 选择的编译器和 objdump。
使用 make 构建；若已有产物可能来自另一工具链，
先确认清理目标，再执行 make clean 和 make。
输出要求：保留构建输出、符号表和入口反汇编，
将源码语义与当前二进制展开对应起来。

[RELY]
仓库根目录：/home/wyl/os_work_lab/os-labs。
根 Makefile 转发到 code/Makefile。
ELF：code/bin/kernel；裸镜像：code/bin/ucore.img。
符号：kern_entry、kern_init、bootstack、bootstacktop。
可使用实际工具链的 readelf、nm 和 objdump。

[GUARANTEE]
交付实际工具版本、构建状态和四个符号地址。
由 bootstacktop−bootstack 核对栈大小。
交付 kern_entry 的机器指令、地址和长度。
解释 la 的实际展开及 tail 是否发生链接器松弛。

[SPECIFICATION]
Pre-Condition：工具可用，构建目标和清理范围已确认。
Post-Condition：能够由当前 ELF 核对入口位置、栈布局及指令展开。
Case 1：la 使用地址计算序列，解释完整序列如何得到栈顶地址。
Case 2：la 经 GOT 间接取地址，说明中间读取位置与最终符号地址。
Case 3：tail 被缩短为跳转，记录实际目标和指令长度。
Requirements：地址和指令展开以当前 ELF 为准；构建与观察结果
存在差异时，先检查工具链和产物来源，保留差异。
```

##### Agent 输出概要

返回当前构建得到的地址和反汇编，用符号差值核对栈大小，说明两条伪指令最终对应哪些机器指令。

#### 步骤 5：单步验证 la 设置 SP

##### 我的提示词

```text
[PROMPT]
任务：使用真实 QEMU/GDB，在 kern_entry 观察 la 的执行，
验证它是否将 SP 设置为 bootstacktop。
操作要求：先给出两个终端的命令，由我执行并提供输出；
根据我实际返回的结果逐步解释，不一次性推进全部调试。
如由 Agent 自动执行，明确标注自动调试来源。
输出要求：记录入口处和 la 每条机器指令执行后的 PC、SP、RA。

[RELY]
使用仓库根目录的 make debug、make gdb。
QEMU 加载 code/bin/ucore.img，GDB 加载 code/bin/kernel，
两者来自上一步同一次构建。
使用 break *kern_entry、continue、disassemble kern_entry、
info registers pc sp ra、si、p/x &bootstacktop。
实际 la 指令序列来自上一步反汇编。

[GUARANTEE]
交付入口断点命中记录、实际入口反汇编，
以及 la 执行过程的寄存器变化表。
执行完整 la 序列后比较 SP 与 bootstacktop。
保留会话在 tail 指令执行前，供下一步继续使用。

[SPECIFICATION]
Pre-Condition：QEMU 暂停启动，GDB 连接的是当前实例，
入口断点尚未被错过，ELF 与镜像一致。
Post-Condition：完整执行 la 后，SP 等于实际 bootstacktop。
Case 1：la 展开为多条指令，记录中间状态，
只在完整序列后判断最终 SP。
Case 2：断点未命中或 SP 不一致，检查连接、符号和产物，
保留实际输出后分析。
Requirements：si 按机器指令执行；不修改寄存器制造预期值；
本轮只观察入口，不展开练习二的复位和固件分析。
```

##### Agent 输出概要

根据真实单步输出，解释 SP 如何从固件遗留状态变为内核栈顶，核对 PC 的变化，并停在 `tail` 对应指令之前。

#### 步骤 6：验证 tail 并解释返回关系

##### 我的提示词

```text
[PROMPT]
任务：继续上一步的调试会话，验证 tail kern_init 的跳转目标，
比较执行前后的 PC、SP、RA，并解释它与 call 的区别。
操作要求：执行前保存 RA，按照实际反汇编执行 tail 对应序列。
到达 kern_init 第一条指令时立即记录状态。
输出要求：给出前后状态表，结合 noreturn 和无限循环解释设计目的。

[RELY]
当前调试会话停在 tail 对应指令之前。
kern_init 地址来自当前 ELF。
可使用 set $old_ra = $ra、si、info registers pc sp ra、
p/x $old_ra、p/x &kern_init。
init.c 中 kern_init 被标记为 noreturn，末尾是无限循环。

[GUARANTEE]
验证 tail 后 PC 等于 kern_init。
验证 tail 序列没有改写 RA，并核对 SP 保持内核栈顶。
解释普通 call 建立返回地址，而此入口无需建立新的返回关系。
完成观察后退出 GDB，并停止本次 QEMU。

[SPECIFICATION]
Pre-Condition：上一轮调试会话与构建产物保持一致，
尚未执行 tail 序列。
Post-Condition：已到达 kern_init 入口，有实际记录支撑
控制权转移和 RA 保持的解释。
Case 1：tail 为单条跳转，执行一条后立即记录。
Case 2：tail 为多条序列，执行完整序列后记录。
Requirements：记录点在 kern_init 函数体执行前；
之后 C 代码可能调整 SP，不能据此否定入口设置；
只清理本次调试实例。
```

##### Agent 输出概要

确认跳入 C 入口时 PC、SP、RA 的状态，解释 `tail`、`call`、`noreturn` 的关系，并结束调试会话。

#### 步骤 7：整理练习一答案与交互记录

##### 我的提示词

```text
[PROMPT]
任务：根据前六步的源码分析、构建、反汇编和单步记录，
整理练习一的最终书面答案。
操作要求：先核对证据是否来自同一版本和构建，
生成 Markdown 建议文本供我审核。
输出要求：依照指导书分别回答两条指令“完成什么操作、
目的是什么”，并提供对应证据；本轮不写入文件。

[RELY]
练习一原文，以及前六步实际取得的宏定义、符号地址、
kern_entry 反汇编和 PC、SP、RA 记录。

[GUARANTEE]
交付以下内容：
1. la 的操作、建立自有栈的目的、栈大小和地址范围；
2. la 的实际展开和 SP 验证结果；
3. tail 的操作、与 call 的差异及不需要返回的原因；
4. tail 的实际展开和 PC、SP、RA 验证结果；
5. 实际遇到的问题、提示词如何调整及复验结果。

[SPECIFICATION]
Pre-Condition：前述证据齐全；缺失项已经明确标注。
Post-Condition：文本完整回答练习一，与当前源码和运行记录一致。
Case 1：源码、反汇编和运行结果一致，按证据整理答案。
Case 2：存在差异，定位原因并复验后再形成结论。
Case 3：没有发生失败迭代，如实记录一次完成，
不编造错误或修复经历。
Requirements：区分计划、Agent 输出与实际完成记录；
交互流程只有真正使用后才能作为实际提示词历史。
```

##### Agent 输出概要

生成练习一报告文本，并整理真实的交互与验证过程。最终答案应同时有源码解释、实际反汇编和寄存器变化作为依据。

#### 问题反馈与迭代

某一步失败时，提供实际命令、完整错误信息、工具版本，以及调试停机位置；要求 Agent 分析原因并明确下一次验证条件。疑问解决后继续当前步骤，完成后再进入下一步骤。

每轮保存实际使用的提示词和执行结果，将本次真实取得的证据记入操作记录。

### Lab1 练习二

#### 步骤 1：明确我的任务与验收证据

##### 我的提示词

```text
[PROMPT]
任务：我负责练习二，使用 QEMU 和 GDB 验证复位流程、OpenSBI 交接、特权级和寄存器状态。请阅读课程要求和 lab1 分支，明确我需要验证的启动过程和验收条件。
操作要求：先只读分析。按“必须完成、支撑性观察、小组已有工具”区分工作，暂不修改内核或填写实验结论。
输出要求：给出任务清单，每项关联源码位置、观察方法和验收证据，并用初学者能理解的语言说明目的。

[RELY]
仓库：https://github.com/hard-male-alliance/os-labs，实验分支 lab1。
课程要求：docs/course/lab1/lab1_2_1_exercise.md、lab1_5_requirement.md。
现有材料：code/、report/evidence/、code/Makefile。
我的任务：QEMU/GDB 启动调试，寄存器与特权级观察。

[GUARANTEE]
交付“任务—源码—调试方法—完成证据”对应表。
列明练习二的问题，区分课程要求与小组额外验证。
没有新增内核函数的要求，不为套用模板虚构函数实现任务。

[SPECIFICATION]
Pre-Condition：确认当前分支与课程要求，不能只根据默认 main 分支判断实验是否存在。
Post-Condition：我能说明要验证什么，以及什么证据可以支撑结论。
Case 1：源码或记录已存在，标注可复用材料及其来源。
Case 2：缺少材料，列出待实际操作项，不填入推测结果。
Requirements：将指导书要求、源码推断和运行观察明确区分。
```

##### Agent 输出概要

说明练习二的观察目标，列出复位指令、OpenSBI 交接、内核入口、启动栈和特权级的验证方法。

#### 步骤 2：核对调试环境与工程入口

##### 我的提示词

```text
[PROMPT]
任务：检查现有工程是否具备执行练习二的条件，确定实际使用的编译器、QEMU 和 GDB，以及启动调试的方法。
操作要求：在实际 Linux/WSL 环境中检查版本，读取根目录和 code/ 下的 Makefile；不擅自安装系统软件。需要改配置时，仅做有依据的最小修改并说明原因。
输出要求：保存真实环境检查记录，解释 make、make debug、make gdb 的职责和工作目录。

[RELY]
实际工程为 lab1 分支，源码目录是 code/。
可读取 Makefile、code/Makefile 和 scripts/check_environment.py。
工具选择可能为 riscv64-unknown-elf-* 或 riscv64-linux-gnu-*，调试器可能为 gdb-multiarch；以工程解析结果为准。

[GUARANTEE]
交付实际工具路径、版本、QEMU 启动参数和 GDB 连接端点。
明确 GDB 加载的 ELF 文件与 QEMU 使用的镜像文件。
现有编译和调试入口保持可用。

[SPECIFICATION]
Pre-Condition：已定位真正包含工程的 Linux/WSL 目录；不把 Windows PowerShell 当成 Bash。
Post-Condition：工具链支持 RISC-V，GDB 能读取内核符号并连接本次 QEMU。
Case 1：工具齐全，给出可执行命令。
Case 2：工具缺失或不兼容，输出实际错误与所缺条件，暂停依赖该条件的调试。
Requirements：版本号必须来自本次实际命令输出。
```

##### Agent 输出概要

列出实际系统与工具版本，说明根目录 Makefile 如何转发到 code/，以及 GDB 使用的 ELF、QEMU 使用的镜像和连接端点。

#### 步骤 3：先验证普通启动，建立对照结果

##### 我的提示词

```text
[PROMPT]
任务：先验证最小内核能够正常编译和启动，作为后续 GDB 调试的对照。
操作要求：使用工程现有 Make 目标运行真实构建和 QEMU，保存命令、退出状态和原始输出。不要先修改内核来获得预期输出。
输出要求：报告是否生成 ELF 和原始镜像，是否出现内核启动文字，以及打印后程序为何持续运行。

[RELY]
code/kern/init/init.c 中 kern_init 清零、打印并进入无限循环。
预期启动文字：(THU.CST) os is loading ...
现有 make smoke 可以辅助检查，执行前先读取其实现。

[GUARANTEE]
交付构建记录和独立的普通启动日志。
区分 OpenSBI 的启动横幅与内核自己的启动文字。

[SPECIFICATION]
Pre-Condition：环境检查通过，镜像来自当前源码构建。
Post-Condition：真实终端输出包含内核启动文字，且构建成功。
Case 1：打印后持续运行，结合无限循环解释这是预期行为。
Case 2：构建失败或没有内核输出，先定位失败阶段，不写“启动成功”。
Requirements：停止本次启动的 QEMU 后再开展调试；没有 tools/grade.sh 时，不声称通过 make grade。
```

##### Agent 输出概要

返回真实构建状态与启动输出，检查是否出现预期内核文字，并结合源码解释打印后保持运行的原因。

#### 步骤 4：制定从复位到内核的调试方案

##### 我的提示词

```text
[PROMPT]
任务：为练习二制定可执行的 GDB 观察方案，覆盖复位代码、OpenSBI 和内核入口。
操作要求：先读取实际 debug/gdb 目标，再给出两个终端的操作顺序。复位代码使用机器指令单步，较长的固件流程使用断点；不要将源代码行单步与机器指令单步混淆。
输出要求：给出每组命令的目的、需要记录的状态和失败时的排查方法，不填入未观察的寄存器值。

[RELY]
关键地址：本实验预期复位 0x1000、OpenSBI 0x80000000、内核 0x80200000。
关键符号：kern_entry、kern_init、bootstack、bootstacktop。
GDB 可使用 info registers、x/i、si、break、continue。

[GUARANTEE]
交付复位观察、固件到达、内核断点和入口单步四组操作。
每组包含“命令、观察项目、验收条件”。

[SPECIFICATION]
Pre-Condition：QEMU 在 CPU 开始执行前暂停，GDB 连接本次实例。
Post-Condition：能够逐阶段确认执行路径，而非仅由最终打印反推过程。
Case 1：地址与预期一致，保存实际观察。
Case 2：初始 PC 或断点结果不同，检查平台、启动参数及是否已执行过该位置。
Requirements：0x1000 是平台事实，不写成 RISC-V 架构统一规定。
```

##### Agent 输出概要

方案先读取初始 PC 和复位反汇编，单步进入固件，再以入口断点跳过大量固件指令，最后观察设置栈和进入 C 函数的过程。

#### 步骤 5：执行复位单步，解释最初六条指令

##### 我的提示词

```text
[PROMPT]
任务：执行复位区域的 GDB 观察，解释 CPU 最初执行的指令如何准备启动参数并跳转到固件。
操作要求：记录初始 PC、六条实际指令及机器码；逐条 si，在每条执行后记录相关寄存器。保留完整原始输出，表格从日志提取。
输出要求：按“地址、指令、执行后状态、作用”整理，并解释 t0、a0、a1、a2 的用途。

[RELY]
调试方案来自上一步，实际指令以当前 QEMU 反汇编为准。
可用 info registers pc t0 a0 a1 a2、x/6i $pc 和内存读取命令。
设备树描述平台硬件；固件动态信息与设备树是不同数据。

[GUARANTEE]
交付初始 PC、六次单步记录和跳转后的 PC。
说明固件目标地址从何处取得，并关联对应的内存读取。

[SPECIFICATION]
Pre-Condition：GDB 连接暂停在复位的 CPU，尚未跳过复位序列。
Post-Condition：能由实际寄存器变化说明启动参数准备和固件跳转。
Case 1：跳转后 PC 为 0x80000000，记录到达固件入口。
Case 2：指令或目标不同，以实际输出为依据检查版本及配置，不强行套用示例。
Requirements：反汇编窗口可能包含数据，只解释实际执行的指令；不要把后续数据误写成执行路径。
```

##### Agent 输出概要

列出初始 PC、六条复位指令及逐步寄存器变化，解释启动参数准备和最终跳转目标。

#### 步骤 6：核对镜像预加载与监视点的含义

##### 我的提示词

```text
[PROMPT]
任务：核对指导书中的 watch *0x80200000 在当前启动方式下能证明什么，避免混淆镜像加载与执行权交接。
操作要求：读取实际 QEMU 启动参数；在复位暂停时检查内核入口内存，条件允许时将完整镜像与客体内存比较。只有实际试过监视点，才能报告其触发情况。
输出要求：解释 QEMU、OpenSBI 在当前配置中的职责，并给出独立验证内核开始执行的方法。

[RELY]
code/Makefile 当前使用 -bios default 和 -kernel 加载镜像。
code/tools/kernel.ld 的链接基址为 0x80200000。
可读取 bin/ucore.img 和对应客体内存；必要时查当前 QEMU 版本的启动源码。

[GUARANTEE]
交付 CPU 仍在复位时的镜像内存检查结果。
明确写监视点与执行断点的区别，结论限定于当前平台和参数。

[SPECIFICATION]
Pre-Condition：比较的是本次构建的镜像，CPU 尚未执行固件。
Post-Condition：能区分“代码已放入内存”和“CPU 已开始执行代码”。
Case 1：复位时镜像已存在，说明之后设置写监视点未必捕捉到加载。
Case 2：镜像不匹配，检查比较地址、镜像格式和加载配置后再下结论。
Requirements：未触发不等于未加载；没有实际 watch 输出时，不虚构“我观察到监视点未触发”。
```

##### Agent 输出概要

返回镜像与客体内存的比较结果，解释当前加载方式，以及写监视点和入口执行断点能够分别验证什么。

#### 步骤 7：验证内核入口、启动栈与 C 入口

##### 我的提示词

```text
[PROMPT]
任务：确认 OpenSBI 将执行权交给 kern_entry，并用运行结果支撑启动栈和 tail 跳转的解释。
操作要求：在实际内核入口设断点并继续执行；读取 ELF 符号，反汇编入口，按机器指令逐步观察 PC、SP、RA。只读调试，不改变内核逻辑。
输出要求：整理入口地址、栈范围、栈大小、实际指令展开及进入 kern_init 后的寄存器状态。

[RELY]
code/kern/init/entry.S：la sp, bootstacktop；tail kern_init。
实际符号：kern_entry、kern_init、bootstack、bootstacktop。
栈宏来自 code/kern/mm/mmu.h 和 memlayout.h，不凭记忆填写。

[GUARANTEE]
交付命中 kern_entry 的记录，确认它与 0x80200000 的对应关系。
验证设置后的 SP 等于 bootstacktop，并由符号差值计算栈大小。
比较 tail 前后的 RA，验证到达 C 入口。

[SPECIFICATION]
Pre-Condition：加载当前 ELF 符号，内核入口断点尚未被错过。
Post-Condition：有实际状态支撑“设置内核栈后进入 C 初始化”。
Case 1：la 展开为多条指令，执行完整序列后才检查最终 SP。
Case 2：指令展开或地址变化，依据当前反汇编更新记录。
Requirements：不把地址加载理解为读取该地址的数据；不将 tail 写成建立新返回地址的普通调用。
```

##### Agent 输出概要

返回实际入口地址、栈底与栈顶、栈大小和入口反汇编，核对 SP 初始化、进入 kern_init 及 RA 是否保持不变。

#### 步骤 8：核对特权级及 SBI 输出的补充证据

##### 我的提示词

```text
[PROMPT]
任务：核对复位与内核入口的特权级，使用小组已有验证工具理解内核如何通过 SBI 输出字符。
操作要求：先审阅 scripts/verify_lab1.py，再使用或核对其实际日志。将本步标为补充观察；如验证失败，依据实际停机位置分析后迭代，不删除失败记录以获得通过。
输出要求：解释 M 模式、S 模式、satp、ecall、mcause 和 mepc 在本次观察中的含义。

[RELY]
已有脚本：scripts/verify_lab1.py；已有日志：report/evidence/verify-gdb.log、verify-result.json。
输出路径包含 cprintf、sbi_console_putchar 和 ecall，接口以实际源码为准。
本次内核使用传统 SBI 字符输出服务，服务号在 a7 中。

[GUARANTEE]
核对复位处 M 模式、内核入口 S 模式及 satp 状态。
核对字符输出 ecall、固件陷入入口及返回后的状态。
说明这是对已有小组工具的使用，不声称该脚本由我独立编写。

[SPECIFICATION]
Pre-Condition：工具实际暴露相关寄存器，日志对应同一被测实例。
Post-Condition：能够用记录解释 S 模式请求 M 模式固件服务再返回的过程。
Case 1：单步 ecall 后已返回 S 模式，不能据此否认曾进入 M 模式；改用实际陷入入口断点观察。
Case 2：寄存器不可见或断点失败，记录限制，不补造状态。
Requirements：satp=0 表示本次未启用地址转换，不表示 CPU 处于 M 模式；检查 mepc 与实际 ecall 地址一致。
```

##### Agent 输出概要

返回复位、内核入口、SBI 陷入和返回时的特权级与寄存器状态，解释服务号、异常原因和返回路径。

#### 步骤 9：运行自动验证并核对实验结果

##### 我的提示词

```text
[PROMPT]
任务：使用小组已有的 make verify 对启动调试结论进行复验，检查结果是否与手动观察一致。
操作要求：先阅读 scripts/verify_lab1.py，识别脚本是否修改客体内存或寄存器，再执行验证。新结果保存在独立目录，避免覆盖已有记录。失败时保留错误、日志和当时状态，分析原因后重新验证。
输出要求：返回执行命令、退出状态、检查结果，以及与前述单步观察的差异。说明哪些属于正常启动，哪些属于额外测试。

[RELY]
已有入口：根目录 make verify。
已有脚本：scripts/verify_lab1.py。
对照信息：本次手动观察的复位 PC、固件入口、内核入口、栈范围和特权级。
源码中 kern_init 调用 memset(edata, 0, end - edata)，BSS 是否非空以实际 ELF 符号为准。

[GUARANTEE]
核对复位位置、固件与内核交接、启动栈、SBI 输出和启动文字。
检查 edata 与 end 的关系，区分原始 BSS 清零和单独的合成清零实验。
验证失败时返回非零退出状态，清理本次启动的 QEMU，不影响其他实例。

[SPECIFICATION]
Pre-Condition：脚本使用本次源码对应的 ELF 和镜像，调试端口属于本次 QEMU。
Post-Condition：自动检查与手动观察一致；存在差异时能定位对应状态和日志。
Case 1：检查通过，列出实际通过的检查数与覆盖范围。
Case 2：检查失败，区分构建、连接、超时和客体状态错误，不跳过断言获得通过。
Case 3：BSS 为空，明确零长度调用的含义；合成实验独立标识并恢复所改状态。
Requirements：不把重复的断点检查计为独立功能；make verify 不等同于课程 make grade；不修改内核逻辑来迎合检查。
```

##### Agent 输出概要

返回本次验证状态与检查结果，核对其与单步观察的一致性，解释 BSS 和合成实验的边界，并确认调试实例已清理。


## 9. 按照实验报告模板生成实验报告

### 我的提示词

```text
[PROMPT]
任务：根据本次 Lab1 的源码分析、练习一和练习二的实际操作，
按照 docs/course-template/实验报告模板.md 生成完整的实验报告。
操作要求：先完整阅读报告模板、Lab1 练习要求及报告要求，
再整理本次构建、反汇编、GDB 调试和测试证据，
将报告写入 report/report.md。保留模板的主要结构，
根据 Lab1 的理解与调试任务调整模块内容。
输出要求：使用简体中文和 Markdown，以清楚的文字说明为主，
必要时使用表格、代码和流程图。完成后逐项检查模板与课程要求的覆盖情况。

[RELY]
报告模板：docs/course-template/实验报告模板.md。
练习要求：docs/course/lab1/lab1_2_1_exercise.md。
报告要求：docs/course/lab1/lab1_5_requirement.md。
交付规范：docs/course-delivery.md。
实验源码：code/，提示词记录：report/prompt.md。
依据：本次实际工具版本、构建输出、ELF 符号与反汇编、
GDB 寄存器记录、QEMU 输出及对应测试截图。
成员姓名、学号和分工以我提供的信息为准。

[GUARANTEE]
生成 report/report.md，覆盖以下内容：
1. 实验基本信息与小组分工；
2. 实验目的；
3. 实验环境与实际使用的 AI 工具；
4. 实验整体逻辑及核心文件、函数的作用；
5. 练习一中 la、tail 的操作、目的和验证证据；
6. 练习二的调试过程、初始指令及启动流程观察；
7. 实际提示词与迭代过程，可引用 prompt.md 中对应内容；
8. 测试与验证结果，以及 report/images/ 中的真实截图；
9. 实验知识与 OS 原理的联系、差异及尚未覆盖的知识点；
10. 实验总结和 AI 协作经验。

[SPECIFICATION]
Pre-Condition：已完整读取模板和课程要求，
用于报告的实验结果具有对应的实际记录。
Post-Condition：报告结构符合模板，完整回答两道练习，
环境、地址、指令和测试结论与所引用证据一致。
Case 1：信息和证据齐全，填写实际内容并关联日志或截图。
Case 2：姓名、学号、分工或模型版本未提供，保留明确的待填写项。
Case 3：存在未完成的操作或缺失的截图，列出待办，
不将其写为已经完成。
Case 4：没有发生代码实现或失败迭代，按真实的分析与调试过程记录，
不为套用模板编造函数修改、错误或修复经历。
Requirements：区分历史证据与本次结果；引用截图时说明来源；
源码缺少 tools/grade.sh 时，不声称通过 make grade；
本次 BSS 为空时，不声称正常启动清零了非空 BSS；
完成后检查图片链接、占位项和要求覆盖情况，不执行 Git 提交或推送。
```

### Agent 输出概要

按照模板生成实验报告，说明两道练习、核心模块和实际验证结果，并列出缺失信息与剩余待办。

