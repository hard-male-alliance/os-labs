> 来源：[用户进程](http://8.135.34.58/lab2026/_book/lab5/lab5_3_2_create_user_process.html)

<span id="用户进程"></span>

### 用户进程

前面的内容已经把内核线程和调度流程搭起来了，接下来要解决的是：系统怎样第一次进入用户态，并让一个真正的用户程序开始执行。

先看一眼原先的 `init_main`。它还是一个很纯粹的内核线程入口，主要做的是打印信息和结束自身，不承担用户进程启动的职责：

``` text
// kern/process/proc.c (原先的 init_main)
static int init_main(void *arg) {
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char *)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");
    return 0;
}
```

在 `proc_init()` 里，系统会先构造 `idleproc`，再由它创建 `init_main` 所在的内核线程。`init_main` 的作用不是直接完成业务逻辑，而是作为启动阶段的过渡点，负责继续创建 `user_main`，并在后续等待用户进程结束。这样一来，内核启动阶段就有了一个明确的“从内核线程过渡到用户程序”的入口。

`user_main` 的职责也很清晰，它会从编译进镜像的用户程序中取出目标二进制，然后调用 `kernel_execve`。这里的关键不在于“取到一个文件名”，而在于“把内核线程切换成用户进程”：一旦 `kernel_execve` 完成上下文构造并返回到 trap 的通用返回路径，`user_main` 就不再是普通内核线程，而会开始执行用户程序入口。

在进入 `user_main` 之前，`proc.c` 里还定义了一组用于发起 `kernel_execve` 的宏。它们的作用是把“用户程序名”和“镜像中对应的二进制符号”联系起来，从而让内核线程能够很方便地选中某个用户程序并启动它。它们本质上是 `kernel_execve` 的上层封装，供 `user_main` 直接调用。

``` text
// kern/process/proc.c
#define __KERNEL_EXECVE(name, binary, size) ({                          \
            cprintf("kernel_execve: pid = %d, name = \"%s\".\n",        \
                    current->pid, name);                                \
            kernel_execve(name, binary, (size_t)(size));                \
        })

#define KERNEL_EXECVE(x) ({                                             \
            extern unsigned char _binary_obj___user_##x##_out_start[],  \
                _binary_obj___user_##x##_out_size[];                    \
            __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,     \
                            _binary_obj___user_##x##_out_size);         \
        })

#define __KERNEL_EXECVE2(x, xstart, xsize) ({                           \
            extern unsigned char xstart[], xsize[];                     \
            __KERNEL_EXECVE(#x, xstart, (size_t)xsize);                 \
        })

#define KERNEL_EXECVE2(x, xstart, xsize)        __KERNEL_EXECVE2(x, xstart, xsize)
```

这组宏的意义在于：`KERNEL_EXECVE(exit)` 这样的写法，会把用户程序 `exit` 对应的镜像符号传给 `kernel_execve`，从而把“选择哪个用户程序”这件事变得更自然。对学生来说，重点不是记住宏展开细节，而是理解 `user_main` 通过这层封装，最终会调用到真正的 `kernel_execve`。

用户程序本身通常通过 `exit`、`fork`、`wait` 等接口验证系统调用链路。用户态库函数的工作是把调用包装成系统调用请求，而不是直接碰内核接口；比如 `cprintf` 需要通过 `sys_putc` 输出字符，因为用户态不能直接调用内核里的控制台接口。这一点也说明了用户态和内核态之间的边界：用户程序只能通过系统调用获得服务，不能绕过权限检查直接访问内核资源。

`user/` 目录下存放的就是这类用户程序。它们会在构建镜像时被编译进去，运行时再由 `kernel_execve` 选中并加载。下面这个 `exit` 程序就是实验中最典型的测试用例之一，它会验证 `fork`、`waitpid` 和 `exit` 这些系统调用是否能够正确协作。

``` text
// user/exit.c
#include <stdio.h>
#include <ulib.h>

int magic = -0x10384;

int main(void) {
    int pid, code;
    cprintf("I am the parent. Forking the child...\n");
    if ((pid = fork()) == 0) {
        cprintf("I am the child.\n");
        yield();
        yield();
        yield();
        yield();
        yield();
        yield();
        yield();
        exit(magic);
    }
    else {
        cprintf("I am parent, fork a child pid %d\n",pid);
    }
    assert(pid > 0);
    cprintf("I am the parent, waiting now..\n");

    assert(waitpid(pid, &code) == 0 && code == magic);
    assert(waitpid(pid, &code) != 0 && wait() != 0);
    cprintf("waitpid %d ok.\n", pid);

    cprintf("exit pass.\n");
    return 0;
}
```

这个程序的作用很直接：先制造父子进程，再通过 `yield` 让调度器来回切换，最后让父进程等待子进程退出并检查退出码。它是验证进程复制、调度和退出链路是否正常的核心测试之一。

这些用户程序依赖的系统调用接口都封装在 `user/libs/ulib.c` 里。它们的设计方式很朴素：用户代码调用的是普通 C 函数，底层真正做事的是 `sys_*` 系统调用封装。

``` text
// user/libs/ulib.c
#include <defs.h>
#include <syscall.h>
#include <stdio.h>
#include <ulib.h>

void exit(int error_code) {
    sys_exit(error_code);
    // 执行完 sys_exit 后，按理说进程就结束了，后面的语句不应该再执行，
    // 所以执行到这里就说明 exit 失败了。
    cprintf("BUG: exit failed.\n");
    while (1);
}

int fork(void) { return sys_fork(); }
int wait(void) { return sys_wait(0, NULL); }
int waitpid(int pid, int *store) { return sys_wait(pid, store); }
void yield(void) { sys_yield(); }
int kill(int pid) { return sys_kill(pid); }
int getpid(void) { return sys_getpid(); }
```

其中 `exit` 这个封装尤其重要，因为它清楚地表达了一个事实：一旦系统调用成功，后面的语句就不应该继续执行，所以如果程序真的运行到了 `cprintf("BUG: exit failed.\n")`，就说明退出路径没有按预期完成。

在用户程序里使用的 `cprintf()` 也是在 `user/libs/stdio.c` 里重新实现的。和内核里的打印接口相比，它最大的不同是不能直接访问底层控制台，而必须通过 `sys_putc()` 进入内核完成输出。

``` text
// user/libs/stdio.c
#include <defs.h>
#include <stdio.h>
#include <syscall.h>

/* *
 * cputch - writes a single character @c to stdout, and it will
 * increace the value of counter pointed by @cnt.
 * */
static void
cputch(int c, int *cnt) {
    sys_putc(c); // 系统调用
    (*cnt) ++;
}

/* *
 * vcprintf - format a string and writes it to stdout
 *
 * The return value is the number of characters which would be
 * written to stdout.
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want cprintf() instead.
 * */
int
vcprintf(const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void*)cputch, &cnt, fmt, ap);
    // 注意这里复用了 vprintfmt, 但是传入了 cputch 函数指针
    return cnt;
}
```

之所以要这样做，是因为用户态没有权限直接调用 `sbi_console_putchar()`。用户程序如果想打印字符，只能先通过系统调用进入内核，再由内核帮它完成控制台输出。

这一章最重要的理解点，是把 `init_main`、`user_main`、`kernel_execve` 和用户程序本身连成一条清晰的启动链：`init_main` 先把系统从纯内核启动推进到准备运行用户程序的阶段，`user_main` 再选中镜像里的目标用户程序，`kernel_execve` 负责把执行流送入返回用户态的路径，最后由用户程序真正开始执行。这样看下来，这一章讲的其实就是“第一个用户程序是怎样被选中、装入并启动起来的”。
