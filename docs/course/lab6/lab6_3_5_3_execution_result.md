> 来源：[stride调度算法执行 make qemu 的大致输出](http://8.135.34.58/lab2026/_book/lab6/lab6_3_5_3_execution_result.html)

<span id="stride调度算法执行make-qemu的大致输出"></span>

## stride调度算法执行`make qemu`的大致输出

``` text
$ make qemu
......
sched class: stride_scheduler
++ setup timer interrupts
kernel_execve: pid = 2, name = "priority".
set priority to 6
main: fork ok,now need to wait pids.
set priority to 5
set priority to 4
set priority to 3
set priority to 2
set priority to 1
child pid 7, acc 944000, time 2010 
child pid 6, acc 788000, time 2010 
child pid 5, acc 620000, time 2010 
child pid 4, acc 460000, time 2020 
child pid 3, acc 316000, time 2020 
main: pid 3, acc 316000, time 2020 
main: pid 4, acc 460000, time 2020 
main: pid 5, acc 620000, time 2030 
main: pid 6, acc 788000, time 2030 
main: pid 7, acc 944000, time 2030 
main: wait pids over
sched result: 1 1 2 2 3 
all user-mode processes have quit. 
init check memory pass.
kernel panic at kern/process/proc.c:<line>:
    initproc exit.
```

其中进程累计执行次数和时间会随运行环境有所波动，但各进程获得的 CPU 时间应大致符合其优先级比例。
