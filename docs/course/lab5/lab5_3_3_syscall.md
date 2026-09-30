> 来源：[系统调用实现](http://8.135.34.58/lab2026/_book/lab5/lab5_3_3_syscall.html)

<span id="系统调用实现"></span>

### 系统调用实现

系统调用是用户态程序获得内核服务的标准入口。用户程序并不直接操作内核数据结构，而是先把系统调用号和参数放入约定位置，再通过 `ecall` 进入异常处理流程，由内核根据当前进程的 trapframe 完成分发。

用户态系统调用的入口已经由我们实现好了，用户程序只要调用这些封装函数即可。

``` text
// libs/unistd.h
/* syscall number */
#define SYS_exit            1
#define SYS_fork            2
#define SYS_wait            3
#define SYS_exec            4
#define SYS_clone           5
#define SYS_yield           10
#define SYS_sleep           11
#define SYS_kill            12
#define SYS_gettime         17
#define SYS_getpid          18
#define SYS_brk             19
#define SYS_mmap            20
#define SYS_munmap          21
#define SYS_shmem           22
#define SYS_putc            30
#define SYS_pgdir           31
```

``` text
// user/libs/syscall.c
#include <defs.h>
#include <unistd.h>
#include <stdarg.h>
#include <syscall.h>
#define MAX_ARGS            5
static inline int syscall(int num, ...) {
    // va_list, va_start, va_arg 都是 C 语言处理参数个数不定的函数的宏
    // 在 stdarg.h 里定义。
    va_list ap; // ap: 参数列表(此时未初始化)
    va_start(ap, num); // 初始化参数列表, 从 num 开始
    uint64_t a[MAX_ARGS];
    int i, ret;
    for (i = 0; i < MAX_ARGS; i ++) {
        a[i] = va_arg(ap, uint64_t);
    }
    va_end(ap);
    asm volatile (
        "ld a0, %1\n"
        "ld a1, %2\n"
        "ld a2, %3\n"
        "ld a3, %4\n"
        "ld a4, %5\n"
        "ld a5, %6\n"
        "ecall\n"
        "sd a0, %0"
        : "=m" (ret)
        : "m"(num), "m"(a[0]), "m"(a[1]), "m"(a[2]), "m"(a[3]), "m"(a[4])
        :"memory");
    return ret;
}
int sys_exit(int error_code) { return syscall(SYS_exit, error_code); }
int sys_fork(void) { return syscall(SYS_fork); }
int sys_wait(int pid, int *store) { return syscall(SYS_wait, pid, store); }
int sys_yield(void) { return syscall(SYS_yield); }
int sys_kill(int pid) { return syscall(SYS_kill, pid); }
int sys_getpid(void) { return syscall(SYS_getpid); }
int sys_putc(int c) { return syscall(SYS_putc, c); }
```

这一章的核心链路可以概括成三层。第一层是用户态封装，它把 `fork`、`wait`、`exec`、`exit`、`yield`、`kill`、`getpid`、`putc` 等接口统一转换成系统调用请求。第二层是异常入口，它识别当前异常是否来自用户态系统调用，如果是，就调整返回地址并进入系统调用分发。第三层是内核语义层，它把系统调用号映射成对应的内核处理函数，例如进程创建、进程退出、等待子进程、程序替换、输出字符以及查看页表等。

理解这部分时，最重要的是分清“参数传递”和“语义执行”这两件事。用户态只负责把请求送进内核，真正的权限检查、地址空间检查、进程状态变化和资源更新都在内核中完成。系统调用返回时，内核会把结果写回到 trapframe 中，用户程序随后继续执行，这就是 `ecall` 和 `sret` 共同形成的完整闭环。

这一部分的知识点也直接连接到后面的进程管理：`fork` 会走向进程复制，`exec` 会走向新程序加载，`exit` 会走向进程退出，`wait` 会走向僵尸回收。把这些接口放在同一条调用链里理解，能更清楚地看出用户进程生命周期是如何被内核组织起来的。
