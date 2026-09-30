> 来源：[练习](http://8.135.34.58/lab2026/_book/lab6/lab6_2_1_exercises.html)

<span id="练习"></span>

### 练习

本实验建立在前面实验已经完成的物理内存、虚拟内存和进程管理基础上，重点理解这些基础如何与调度器框架衔接。学习过程可以沿着“理解调度框架、实现 Round Robin、切换并实现 Stride”的顺序展开：先认识调度类和运行队列如何组织一次调度，再通过时间片轮转观察进程交替运行，最后结合 Stride 调度理解优先级与 CPU 时间分配之间的关系。

本实验依赖实验2/3/4/5。请使用你此前设计的提示词生成相应实验代码，并将生成结果填写到本实验代码中标有“lab2”/“lab3”/“lab4”/“lab5”注释的对应位置，确保代码能够正确编译。

补回前序实验代码后，还需要按照本实验的更新标记完成 `alloc_proc()` 和 `interrupt_handler()` 的 Lab 6 更新。

<span id="功能模块一：调度器框架准备、代码更新与理解（设计提示词借助-ai-完成）"></span>

#### 功能模块一：调度器框架准备、代码更新与理解（设计提示词借助 ai 完成）

<span id="任务一：更新进程控制块初始化（设计提示词借助-ai-完成）"></span>

##### 任务一：更新进程控制块初始化（设计提示词借助 ai 完成）

要求实现的函数有：

``` text
static struct proc_struct *alloc_proc(void); (kern/process/proc.c)
```

在补回前序实验代码后，请设计 `prompt`，让 AI 在 `kern/process/proc.c` 中标有 `//lab6 function1-1 YOUR CODE update alloc_proc` 的位置完成 Lab 6 更新。需要初始化调度器使用的 `rq`、`run_link`、`time_slice`、`lab6_run_pool`、`lab6_stride` 和 `lab6_priority` 字段，使新建进程进入调度框架前具有确定的初始状态。

之所以需要更新 `alloc_proc()`，是因为 Lab 6 在进程控制块中加入了运行队列、时间片和 Stride 调度相关字段。若这些字段没有在创建进程时初始化，调度器后续进行入队、出队或选择进程时可能使用未定义的链表节点、斜堆节点或调度参数。更新思路是在保留前序实验初始化代码的基础上，分别将运行队列指针置空、初始化链表和斜堆节点，并将时间片、Stride 和优先级设置为合适的初始值。

整个任务的注意事项为：不能删除或改写前序实验对其他进程控制块字段的初始化；调度字段必须与 `struct proc_struct` 的实际定义一致，链表节点和斜堆节点不能保留未定义内容；初始化逻辑不能提前把进程加入运行队列。

<span id="任务二：更新时钟中断处理（设计提示词借助-ai-完成）"></span>

##### 任务二：更新时钟中断处理（设计提示词借助 ai 完成）

要求实现的函数有：

``` text
void interrupt_handler(struct trapframe *tf); (kern/trap/trap.c)
```

在保留实验三中断处理代码的基础上，请设计 `prompt`，让 AI 在 `kern/trap/trap.c` 中标有 `//lab6 function1-2 YOUR CODE update interrupt_handler` 的位置完成 Lab 6 更新。`IRQ_S_TIMER` 分支需要重新设置下一次时钟事件、更新时钟计数，并通过 `sched_class_proc_tick(current)` 将时钟节拍交给当前调度类处理，使 RR 或 Stride 能够更新进程时间片和 `need_resched`。

之所以需要更新 `interrupt_handler()`，是因为 Lab 6 的 RR 和 Stride 调度都依赖时钟中断驱动时间片消耗；如果时钟中断没有把节拍传递给调度框架，进程就不会及时触发重新调度。更新思路是在实验三已有的定时器分支中继续设置下一次时钟事件、递增时钟计数，并在 `current` 有效时调用统一的 `sched_class_proc_tick(current)`，由当前调度类决定如何处理时间片，而不是在中断处理函数中直接实现某一种调度算法。

整个任务的注意事项为：不能删除其他中断原因的处理逻辑，不能在中断处理函数中直接写死某一种调度算法；应保持时钟事件能够持续触发，并在 `current` 有效时调用调度框架提供的节拍接口。

<span id="任务三：理解调度器框架的实现（理解题）"></span>

##### 任务三：理解调度器框架的实现（理解题）

本任务用于理解 ucore 如何把通用调度流程与具体调度策略分开。请仔细阅读调度器框架的实现，并在实验报告中分别完成以下分析：

- **调度类结构体分析**：解释 `struct sched_class` 中每个函数指针的作用和调用时机，并分析为什么需要使用函数指针组织这些调度接口，而不是直接实现固定的调度函数。
- **运行队列结构体分析**：比较 `lab5` 和 `lab6` 中 `struct run_queue` 的差异，说明为什么 `lab6` 的运行队列需要同时支持链表和斜堆两种数据结构。
- **调度器框架函数分析**：分析 `sched_init()`、`wakeup_proc()` 和 `schedule()` 在 `lab6` 中的实现变化，说明这些函数如何与具体的调度算法解耦。
- **调度类初始化流程**：描述从内核启动到调度器初始化完成的完整流程，并分析 `default_sched_class` 如何与调度器框架建立关联。
- **进程调度流程**：绘制完整的进程调度流程图，包括时钟中断触发、`proc_tick` 被调用、`schedule()` 执行以及调度类各接口的调用顺序，并解释 `need_resched` 标志在调度过程中的作用。
- **调度算法切换机制**：分析添加新的调度算法（如 Stride）时需要修改哪些代码，并说明当前的框架设计为什么便于切换调度算法。

<span id="功能模块二：round-robin-调度算法（设计提示词借助-ai-完成）"></span>

#### 功能模块二：Round Robin 调度算法（设计提示词借助 ai 完成）

要求实现的函数有：

``` text
static void RR_init(struct run_queue *rq);
static void RR_enqueue(struct run_queue *rq, struct proc_struct *proc);
static void RR_dequeue(struct run_queue *rq, struct proc_struct *proc);
static struct proc_struct *RR_pick_next(struct run_queue *rq);
static void RR_proc_tick(struct run_queue *rq, struct proc_struct *proc);
```

完成功能模块一后，请在理解调度器框架的基础上设计`prompt`，让 AI 在`kern/schedule/default_sched.c`中标有`//lab6 function2 YOUR CODE`的位置生成完整的时间片轮转（Round Robin）调度模块。设计提示词时，可以重点阅读`sched.h`中的`struct sched_class`，理解其成员及各调度接口的作用。

整个功能模块的具体功能为：生成的实现应完整处理运行队列初始化、可运行进程入队和出队、下一进程选择，以及时钟中断到来后的时间片更新和重新调度标记。`RR_init`负责初始化运行队列，`RR_enqueue`和`RR_dequeue`负责维护可运行进程，`RR_pick_next`负责选择队首进程，`RR_proc_tick`负责消耗当前进程的时间片。实现完成后还需要定义`default_sched_class`，将这些函数接入统一调度框架。

整个功能模块的注意事项为：实现时应正确维护运行队列、进程所属队列、进程数量和时间片等状态，并使用框架提供的链表、进程转换宏和断言等机制。需要妥善处理空队列、重复入队、时间片无效或耗尽以及空闲进程等边界情况；修改队列时，`run_link`、`rq`和`proc_num`必须保持一致，时间片耗尽时还应正确设置`need_resched`。最终生成的代码应能够正常编译并通过`make grade`测试。

整个功能模块应该满足的对外接口为：RR 调度器通过`default_sched_class`向通用调度框架提供初始化、入队、出队、选择下一进程和处理时钟节拍五项操作。`RR_pick_next`应在队列非空时返回下一个可运行进程，在队列为空时返回`NULL`；其余函数应在不破坏框架状态的前提下完成各自操作。

整个功能模块可以借助的接口函数、结构体、数据结构为：可以重点阅读`sched.h`中的`struct sched_class`和`struct run_queue`、`proc.h`中的`struct proc_struct`，并使用`list_init()`、`list_add_before()`、`list_del_init()`、`list_next()`、`list_empty()`、`le2proc()`和`assert()`等已有接口。`run_list`、`proc_num`、`max_time_slice`、`run_link`、`rq`、`time_slice`和`need_resched`等字段共同记录 RR 调度过程中的状态。

Tips：如果对 RR 调度器如何接入现有框架还不熟悉，可以仔细阅读实验手册“RR调度算法实现”一节，并留意`kern/schedule/sched.c`中`sched_init()`选择和初始化当前调度类的过程。

请在实验报告中完成：

- 提交本任务使用的完整`prompt`。
- 比较一个在`lab5`和`lab6`中均存在但实现不同的函数，说明为什么需要这一改动，以及不修改可能产生的问题。可以参考`kern/schedule/sched.c`，也可以选择其他函数。
- 描述你的提示词让 AI 生成的 RR 调度模块如何实现各项调度接口，解释关键链表操作和**边界情况**的处理方式。
- 展示`make grade`的**输出结果**，并描述在 QEMU 中观察到的调度现象。
- 分析 Round Robin 调度算法的优缺点，并讨论时间片大小对系统性能的影响。
- **拓展思考**：如果要让 AI 生成支持优先级的 RR 调度器，你会如何修改提示词？当前生成的实现是否支持多核调度？如果不支持，提示词还需要增加哪些要求？

<span id="功能模块三：stride-scheduling-调度算法（设计提示词借助-ai-完成）"></span>

#### 功能模块三：Stride Scheduling 调度算法（设计提示词借助 ai 完成）

要求实现的函数有：

``` text
static int proc_stride_comp_f(void *a, void *b);
static void stride_init(struct run_queue *rq);
static void stride_enqueue(struct run_queue *rq, struct proc_struct *proc);
static void stride_dequeue(struct run_queue *rq, struct proc_struct *proc);
static struct proc_struct *stride_pick_next(struct run_queue *rq);
static void stride_proc_tick(struct run_queue *rq, struct proc_struct *proc);
```

请根据后续实验手册对 Stride Scheduling 的介绍设计`prompt`，让 AI 在`kern/schedule/default_sched_stride.c`中标有`//lab6 function3 YOUR CODE`的位置生成完整的 Stride 调度模块。

整个功能模块的具体功能为：`proc_stride_comp_f`负责比较两个进程的 stride，`stride_init`负责初始化运行队列，`stride_enqueue`和`stride_dequeue`负责维护可运行进程，`stride_pick_next`负责选择当前 stride 最小的进程并更新其 stride，`stride_proc_tick`负责更新时间片和重新调度标记。实现完成后还需要定义`stride_sched_class`，将这些函数接入现有调度框架。

整个功能模块的注意事项为：生成的实现应根据进程优先级维护 stride 和步长，并正确处理无符号整数溢出、优先级为零、空运行队列、时间片耗尽和运行队列状态同步等情况。使用斜堆时，应保持`lab6_run_pool`、`rq`和`proc_num`等状态一致；选择进程后，应按照优先级更新其 stride。完成后需要通过编译和运行测试，结合`priority.c`观察不同优先级进程获得的 CPU 时间是否大致符合优先级比例。

整个功能模块应该满足的对外接口为：Stride 调度器通过`stride_sched_class`提供初始化、入队、出队、选择下一进程和处理时钟节拍五项操作。`stride_pick_next`应返回当前 stride 最小的可运行进程，运行队列为空时返回`NULL`；其余接口应正确维护进程时间片及运行队列状态，使通用调度框架无须了解 Stride 的内部实现。

整个功能模块可以借助的接口函数、结构体、数据结构为：可以使用`sched.h`中的`struct sched_class`和`struct run_queue`、`proc.h`中的`struct proc_struct`，以及`skew_heap_insert()`、`skew_heap_remove()`、`skew_heap_init()`、`le2proc()`和`assert()`等已有接口。`lab6_run_pool`、`lab6_stride`、`lab6_priority`、`time_slice`、`need_resched`和`BIG_STRIDE`共同决定 Stride 调度器的队列组织与调度结果。

Tips：完成 Stride 调度模块后，还需要在`sched_init()`中切换调度方法，选择`stride_sched_class`作为当前调度类，否则系统运行的仍是 RR 调度器。如果对算法或数据结构还不熟悉，可以先阅读实验手册“stride调度算法”下的“基本思路”“使用优先队列实现 Stride Scheduling”和执行结果示例。

后面的实验文档部分给出了Stride调度算法的大体描述。这里给出Stride调度算法的一些相关的资料（目前网上中文的资料比较欠缺）。

- [strid-shed paper location](http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.138.3502&rank=1)
- 也可 GOOGLE “Stride Scheduling” 来查找相关资料

请在实验报告中完成：

- 提交本任务使用的完整`prompt`。
- 说明提示词如何描述 Stride 调度规则、运行队列数据结构、`stride`溢出处理和调度框架接口，并分析 AI 生成结果是否满足这些要求。
- 简要证明或说明（不必特别严谨，但应当能够“说服你自己”），为什么 Stride 算法中，经过足够多的时间片之后，每个进程分配到的时间片数目和优先级成正比。

<span id="扩展练习-challenge-1：实现其他基本调度算法"></span>

#### 扩展练习 Challenge 1：实现其他基本调度算法

在 ucore 上实现尽可能多的各种基本调度算法（FIFO、SJF 等），并设计各种测试用例，能够定量分析各种调度算法在不同指标上的差异，说明各调度算法的适用范围。

<span id="扩展练习-challenge-2：linux-的-cfs-调度算法（感兴趣的同学可以学习并实现，不计入成绩）"></span>

#### 扩展练习 Challenge 2：Linux 的 CFS 调度算法（感兴趣的同学可以学习并实现，不计入成绩）

在ucore的调度器框架下也可以实现Linux的CFS调度算法。可阅读相关Linux内核书籍或查询网上资料，这里给出CFS调度算法的一些相关的资料。

- [CFS 调度器 - Linux 内核文档](https://docs.linuxkernel.org.cn/scheduler/sched-design-CFS.html)
- [Linux完全公平调度(CFS)深度解剖（安卓流畅度核心）](https://www.cnblogs.com/16msyanjiusuo/articles/18720910)
- [一文搞懂linux cfs调度器](https://zhuanlan.zhihu.com/p/556295381)

可通过这些相关资料了解CFS的细节，然后大致实现在ucore中。（可以作为Challenge 2的实现）
