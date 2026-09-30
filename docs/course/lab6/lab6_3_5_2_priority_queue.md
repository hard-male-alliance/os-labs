> 来源：[使用优先队列实现 Stride Scheduling](http://8.135.34.58/lab2026/_book/lab6/lab6_3_5_2_priority_queue.html)

<span id="使用优先队列实现-stride-scheduling"></span>

#### 使用优先队列实现 Stride Scheduling

在上述的实现描述中，对于每一次pick_next函数，我们都需要完整地扫描来获得当前最小的stride及其进程。这在进程非常多的时候是非常耗时和低效的，有兴趣的同学可以在实现了基于列表扫描的Stride调度器之后比较一下priority程序在Round-Robin及Stride调度器下各自的运行时间。考虑到其调度选择于优先队列的抽象逻辑一致，我们考虑使用优化的优先队列数据结构实现该调度。

优先队列是这样一种数据结构：使用者可以快速的插入和删除队列中的元素，并且在预先指定的顺序下快速取得当前在队列中的最小（或者最大）值及其对应元素。可以看到，这样的数据结构非常符合 Stride 调度器的实现。

本次实验提供了libs/skew_heap.h作为优先队列的一个实现，该实现定义相关的结构和接口，其中主要包括：

``` text
   // 优先队列节点的结构
   typedef struct skew_heap_entry  skew_heap_entry_t;
   // 初始化一个队列节点
   void skew_heap_init(skew_heap_entry_t *a);
   // 将节点 b 插入至以节点 a 为队列头的队列中去，返回插入后的队列
   skew_heap_entry_t  *skew_heap_insert(skew_heap_entry_t  *a,
                                        skew_heap_entry_t  *b,
                                        compare_f comp);
   // 将节点 b 插入从以节点 a 为队列头的队列中去，返回删除后的队列
      skew_heap_entry_t  *skew_heap_remove(skew_heap_entry_t  *a,
                                           skew_heap_entry_t  *b,
                                           compare_f comp);
```

其中优先队列的顺序由比较函数`comp`决定。对于 Stride 调度器，比较函数需要根据进程的 stride 判断先后顺序，并结合前文介绍的方法正确处理无符号整数溢出。当使用优先队列实现 Stride 调度器后，运行队列结构也需要作相应改变，其中包括：

- struct run_queue中的lab6_run_pool指针，在使用优先队列的实现中表示当前优先队列的头元素，如果优先队列为空，则其指向空指针（NULL）。

- struct proc_struct中的lab6_run_pool结构，表示当前进程对应的优先队列节点。ucore 已经在进程和运行队列结构中预留了 Stride 调度需要的数据，使用优先队列组织这些数据时，可以将其工作过程概括如下：

- init(rq):

  - Initialize rq-\>run_list
  - Set rq-\>lab6_run_pool to NULL
  - Set rq-\>proc_num to 0

- enqueue(rq, proc)

  - Initialize proc-\>time_slice
  - Insert proc-\>lab6_run_pool into rq-\>lab6_run_pool
  - rq-\>proc_num ++

- dequeue(rq, proc)

  - Remove proc-\>lab6_run_pool from rq-\>lab6_run_pool
  - rq-\>proc_num --

- pick_next(rq)

  - If rq-\>lab6_run_pool == NULL, return NULL
  - Find the proc corresponding to the pointer rq-\>lab6_run_pool
  - proc-\>lab6_stride += BIG_STRIDE / proc-\>lab6_priority
  - Return proc

- proc_tick(rq, proc):

  - If proc-\>time_slice \> 0, proc-\>time_slice --
  - If proc-\>time_slice == 0, set the flag proc-\>need_resched

上述过程与 RR 调度器使用相同的调度类接口，区别主要在于运行队列的组织方式和下一进程的选择规则。优先队列使调度器能够直接取得 stride 最小的进程，从而避免每次调度都扫描整个可运行进程集合。
