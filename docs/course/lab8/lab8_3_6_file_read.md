> 来源：[read 系统调用的执行过程](http://8.135.34.58/lab2026/_book/lab8/lab8_3_6_file_read.html)

<span id="read-系统调用的执行过程"></span>

### read 系统调用的执行过程

读文件是从已经打开的普通文件中读取数据。用户进程有如下语句：

``` text
read(fd, data, len);
```

即读取`fd`对应文件，读取长度为`len`，存入`data`中。下面来分析一下读文件的实现。

<span id="通用文件访问接口层的处理流程"></span>

#### 通用文件访问接口层的处理流程

先进入通用文件访问接口层的处理流程，即进一步调用如下用户态函数：`read->sys_read->syscall`，从而引起系统调用进入到内核态。

``` text
int
read(int fd, void *base, size_t len) {
    return sys_read(fd, base, len);
}
```

到了内核态以后，通过中断处理例程，会调用到`sys_read`内核函数，并进一步调用`sysfile_read`内核函数，进入到文件系统抽象层处理流程完成进一步读文件的操作。

``` text
static int
sys_read(uint64_t arg[]) {
    int fd = (int)arg[0];
    void *base = (void *)arg[1];
    size_t len = (size_t)arg[2];
    return sysfile_read(fd, base, len);
}
```

<span id="文件系统抽象层的处理流程"></span>

#### 文件系统抽象层的处理流程

1\) 检查错误，即检查读取长度是否为0和文件是否可读。

2\) 分配`buffer`空间，即调用`kmalloc`函数分配4096字节的`buffer`空间。

3\) 读文件过程

**1. 实际读文件**

循环读取文件，每次读取`buffer`大小。每次循环中，先检查剩余部分大小，若其小于4096字节，则只读取剩余部分的大小。然后调用`file_read`函数（详细分析见后）将文件内容读取到`buffer`中，`alen`为实际大小。调用`copy_to_user`函数将读到的内容拷贝到用户的内存空间中，调整各变量以进行下一次循环读取，直至指定长度读取完成。最后函数调用层层返回至用户程序，用户程序收到了读到的文件内容。

``` text
int
sysfile_read(int fd, void *base, size_t len) {
    struct mm_struct *mm = current->mm;
    if (len == 0) {
        return 0;
    }
    if (!file_testfd(fd, 1, 0)) {
        return -E_INVAL;
    }
    void *buffer;
    if ((buffer = kmalloc(IOBUF_SIZE)) == NULL) {
        return -E_NO_MEM;
    }

    int ret = 0;
    size_t copied = 0, alen;
    while (len != 0) {
        if ((alen = IOBUF_SIZE) > len) {
            alen = len;
        }
        ret = file_read(fd, buffer, alen, &alen);
        if (alen != 0) {
            lock_mm(mm);
            {
                if (copy_to_user(mm, base, buffer, alen)) {
                    assert(len >= alen);
                    base += alen, len -= alen, copied += alen;
                }
                else if (ret == 0) {
                    ret = -E_INVAL;
                }
            }
            unlock_mm(mm);
        }
        if (ret != 0 || alen == 0) {
            goto out;
        }
    }

out:
    kfree(buffer);
    if (copied != 0) {
        return copied;
    }
    return ret;
}
```

**2. `file_read`函数**

这个函数是读文件的核心函数。函数有4个参数，`fd`是文件描述符，`base`是缓存的基地址，`len`是要读取的长度，`copied_store`存放实际读取的长度。函数首先调用`fd2file`函数找到对应的`file`结构，并检查是否可读。调用`fd_array_acquire`函数使打开这个文件的计数加1。调用`vop_read`函数将文件内容读到`iob`中（详细分析见后）。调整文件指针偏移量`pos`的值，使其向后移动实际读到的字节数`iobuf_used(iob)`。最后调用`fd_array_release`函数使打开这个文件的计数减1，若打开计数为0，则释放`file`。

``` text
// read file
int
file_read(int fd, void *base, size_t len, size_t *copied_store) {
    int ret;
    struct file *file;
    *copied_store = 0;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    if (!file->readable) {
        return -E_INVAL;
    }
    fd_array_acquire(file);

    struct iobuf __iob, *iob = iobuf_init(&__iob, base, len, file->pos);
    ret = vop_read(file->node, iob);

    size_t copied = iobuf_used(iob);
    if (file->status == FD_OPENED) {
        file->pos += copied;
    }
    *copied_store = copied;
    fd_array_release(file);
    return ret;
}
```

<span id="sfs文件系统层的处理流程"></span>

#### SFS文件系统层的处理流程

`vop_read`函数实际上是对`sfs_read`的包装。在`sfs_inode.c`中`sfs_node_fileops`变量定义了`.vop_read = sfs_read`，所以下面来分析`sfs_read`函数的实现。

``` text
static int
sfs_read(struct inode *node, struct iobuf *iob) {
    return sfs_io(node, iob, 0);
}
```

`sfs_read`函数调用`sfs_io`函数。它有三个参数，`node`是对应文件的`inode`，`iob`是缓存，`write`表示是读还是写的布尔值（0表示读，1表示写），这里是0。函数先找到`inode`对应`sfs`和`sin`，然后调用`sfs_io_nolock`函数进行读取文件操作，最后调用`iobuf_skip`函数调整`iobuf`的指针。

``` text
/*
 * sfs_io - Rd/Wr file. the wrapper of sfs_io_nolock
            with lock protect
 */
static inline int
sfs_io(struct inode *node, struct iobuf *iob, bool write) {
    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    int ret;
    lock_sin(sin);
    {
        size_t alen = iob->io_resid;
        ret = sfs_io_nolock(sfs, sin, iob->io_base, iob->io_offset, &alen, write);
        if (alen != 0) {
            iobuf_skip(iob, alen);
        }
    }
    unlock_sin(sin);
    return ret;
}
```

在`sfs_io_nolock`函数中完成操作如下：

1.  先计算一些辅助变量，并处理一些特殊情况（比如越界），然后根据`write`参数设置缓冲区和整块读写操作：读操作使用`sfs_rbuf`、`sfs_rblock`，写操作使用`sfs_wbuf`、`sfs_wblock`。
2.  接着进行实际操作，先处理起始的没有对齐到块的部分，再以块为单位循环处理中间的部分，最后处理末尾剩余的部分。
3.  每部分中都调用`sfs_bmap_load_nolock`函数得到`blkno`对应的`inode`编号，并调用相应的缓冲区或整块读写函数处理数据（中间部分使用整块操作，起始和末尾部分使用带偏移的缓冲区操作），调整相关变量。
4.  完成后如果`offset + alen > din->size`（写文件时会出现这种情况，读文件时不会出现这种情况，`alen`为实际读写的长度），则调整文件大小为`offset + alen`并设置`dirty`变量。`sfs_io_nolock`的完整实现由本实验练习生成，这里不再展示函数框架或留白代码。

`sfs_bmap_load_nolock`函数将对应`sfs_inode`的第`index`个索引指向的block的索引值取出存到相应的指针指向的单元（`ino_store`）。它调用`sfs_bmap_get_nolock`来完成相应的操作。`sfs_rbuf`和`sfs_rblock`函数最终都调用`sfs_rwblock_nolock`函数完成操作，而`sfs_rwblock_nolock`函数调用`dop_io->disk0_io->disk0_read_blks_nolock->ide_read_secs`完成对磁盘的操作。

``` text
static int
sfs_bmap_load_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, uint32_t index, uint32_t *ino_store) {
    struct sfs_disk_inode *din = sin->din;
    assert(index <= din->blocks);
    int ret;
    uint32_t ino;
    bool create = (index == din->blocks);
    if ((ret = sfs_bmap_get_nolock(sfs, sin, index, create, &ino)) != 0) {
        return ret;
    }
    assert(sfs_block_inuse(sfs, ino));
    if (create) {
        din->blocks ++;
    }
    if (ino_store != NULL) {
        *ino_store = ino;
    }
    return 0;
}
```
