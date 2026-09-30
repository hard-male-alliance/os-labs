> 来源：[实验内容](http://8.135.34.58/lab2026/_book/lab8/lab8_2_labs.html)

<span id="实验内容"></span>

## 实验内容

实验八在实验七的进程、虚拟内存和同步机制基础上加入文件系统，围绕“文件读写”和“从文件系统加载并执行用户程序”两条主线展开。前一条主线要求理解 `sys_read -> sysfile_read -> file_read -> vop_read -> sfs_read -> sfs_io -> sfs_io_nolock` 的分层调用关系，并生成 SFS 文件读写的核心实现；后一条主线要求理解 `fork -> do_execve -> load_icode -> 用户态返回` 的执行过程，并生成从 ELF 文件建立用户地址空间、用户栈和执行现场的完整实现。

本实验需要完成两处核心功能代码：在 `kern/fs/sfs/sfs_inode.c` 中生成完整的 `sfs_io_nolock()`，在 `kern/process/proc.c` 中生成完整的 `load_icode()`。除此之外，文件描述符和用户地址空间能够正常工作的前提是更新 `alloc_proc()`、`proc_run()` 和 `do_fork()`，这些位置分别负责初始化 `filesp`、切换进程页表并刷新 TLB、复制父进程文件描述符状态。

与实验七相比，实验八增加了 VFS、SFS、设备文件和进程文件表，因此执行程序不再从内存中的二进制缓冲区加载，而是先通过文件系统打开 ELF 文件，再由 `load_icode()` 从文件描述符中读取程序内容。最终应能够启动文件系统中的 `sh`，并在 Shell 中执行 `exit`、`hello` 等用户程序。
