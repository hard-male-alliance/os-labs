> 来源：[open 系统调用的执行过程](http://8.135.34.58/lab2026/_book/lab8/lab8_3_5_file_open.html)

<span id="open-系统调用的执行过程"></span>

### open 系统调用的执行过程

下面我们通过打开文件的系统调用`open()`的执行过程, 看看文件系统的不同层次是如何交互的。

<span id="通用文件访问接口层的处理流程"></span>

#### **通用文件访问接口层的处理流程**

首先，经过`syscall.c`的处理之后，进入内核态，执行`sysfile_open()`函数

``` text
// kern/fs/sysfile.c
/* sysfile_open - open file */
int sysfile_open(const char *__path, uint32_t open_flags) {
    int ret;
    char *path;
    if ((ret = copy_path(&path, __path)) != 0) {
        return ret;
    }
    ret = file_open(path, open_flags);
    kfree(path);
    return ret;
}
```

到了这里，需要把位于用户空间的字符串`__path`拷贝到内核空间中的字符串`path`中，然后调用了`file_open`， `file_open`调用了`vfs_open`， 使用了VFS的接口,进入到文件系统抽象层的处理流程完成进一步的打开文件操作中。

<span id="文件系统抽象层的处理流程"></span>

#### **文件系统抽象层的处理流程**

1、分配一个空闲的`file`数据结构变量`file`在文件系统抽象层的处理中，首先调用的是`file_open`函数，它要给这个即将打开的文件分配一个`file`数据结构的变量，这个变量其实是当前进程的打开文件数组`current->filesp->fd_array[]`中的一个空闲元素（即还没用于一个打开的文件），而这个元素的索引值就是最终要返回到用户进程并赋值给变量`fd`。到了这一步还仅仅是给当前用户进程分配了一个`file`数据结构的变量，还没有找到对应的文件索引节点。

``` text
// kern/fs/file.c
// open file
int file_open(char *path, uint32_t open_flags) {
    bool readable = 0, writable = 0;
    switch (open_flags & O_ACCMODE) { //解析 open_flags
    case O_RDONLY: readable = 1; break;
    case O_WRONLY: writable = 1; break;
    case O_RDWR:
        readable = writable = 1;
        break;
    default:
        return -E_INVAL;
    }
    int ret;
    struct file *file;
    if ((ret = fd_array_alloc(NO_FD, &file)) != 0) { //在当前进程分配file descriptor
        return ret; 
    }
    struct inode *node;
    if ((ret = vfs_open(path, open_flags, &node)) != 0) { //打开文件的工作在vfs_open完成
        fd_array_free(file); //打开失败，释放file descriptor
        return ret;
    }
    file->pos = 0;
    if (open_flags & O_APPEND) {
        struct stat __stat, *stat = &__stat;
        if ((ret = vop_fstat(node, stat)) != 0) {
            vfs_close(node);
            fd_array_free(file);
            return ret;
        }
        file->pos = stat->st_size; //追加写模式，设置当前位置为文件尾
    }
    file->node = node;
    file->readable = readable;
    file->writable = writable;
    fd_array_open(file); //设置该文件的状态为“打开”
    return file->fd;
}
```

为此需要进一步调用`vfs_open`函数来找到`path`指出的文件所对应的基于`inode`数据结构的VFS索引节点`node`。`vfs_open`函数需要完成两件事情：通过`vfs_lookup`找到`path`对应文件的`inode`；调用`vop_open`函数打开文件。`vfs_open`是一个比较复杂的函数，这里我们使用的打开文件的`flags`， 基本是参照linux，如果希望详细了解，可以阅读[linux manual: open](https://man7.org/linux/man-pages/man2/open.2.html)。

``` text
// kern/fs/vfs/vfsfile.c

// open file in vfs, get/create inode for file with filename path.
int vfs_open(char *path, uint32_t open_flags, struct inode **node_store) {
    bool can_write = 0;
    // 解析open_flags并做合法性检查
    switch (open_flags & O_ACCMODE) {
    case O_RDONLY:
        break;
    case O_WRONLY:
    case O_RDWR:
        can_write = 1;
        break;
    default:
        return -E_INVAL;
    }

    if (open_flags & O_TRUNC) {
        if (!can_write) {
            return -E_INVAL;
        }
    }
/*
linux manual
       O_TRUNC
              If the file already exists and is a regular file and the
              access mode allows writing (i.e., is O_RDWR or O_WRONLY) it
              will be truncated to length 0.  If the file is a FIFO or ter‐
              minal device file, the O_TRUNC flag is ignored.  Otherwise,
              the effect of O_TRUNC is unspecified.
*/
    int ret; 
    struct inode *node;
    bool excl = (open_flags & O_EXCL) != 0;
    bool create = (open_flags & O_CREAT) != 0;
    ret = vfs_lookup(path, &node); // vfs_lookup根据路径构造inode

    if (ret != 0) {//要打开的文件还不存在，可能出错，也可能需要创建新文件
        if (ret == -E_NOENT && (create)) {
            char *name;
            struct inode *dir;
            if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0) {
                return ret;//需要在已经存在的目录下创建文件，目录不存在，则出错
            }
            ret = vop_create(dir, name, excl, &node);//创建新文件
        } else return ret;
    } else if (excl && create) {
        return -E_EXISTS;
        /*
        linux manual
              O_EXCL Ensure that this call creates the file: if this flag is
              specified in conjunction with O_CREAT, and pathname already
              exists, then open() fails with the error EEXIST.
        */
    }
    assert(node != NULL);

    if ((ret = vop_open(node, open_flags)) != 0) { 
        vop_ref_dec(node);
        return ret;
    }

    vop_open_inc(node);
    if (open_flags & O_TRUNC || create) {
        if ((ret = vop_truncate(node, 0)) != 0) {
            vop_open_dec(node);
            vop_ref_dec(node);
            return ret;
        }
    }
    *node_store = node;
    return 0;
}
```

`vfs_lookup`函数是一个针对目录的操作函数，它会调用`vop_lookup`函数来找到SFS文件系统中的目录下的文件。为此，`vfs_lookup`函数首先调用`get_device`函数，并进一步调用`vfs_get_bootfs`函数（其实调用了）来找到根目录`“/”`对应的`inode`。这个`inode`就是位于`vfs.c`中的`inode`变量`bootfs_node`。这个变量在`init_main`函数（位于`kern/process/proc.c`）执行时获得了赋值。通过调用`vop_lookup`函数来查找到根目录`“/”`下对应文件`sfs_filetest1`的索引节点，，如果找到就返回此索引节点。

``` text
/*
 * get_device- Common code to pull the device name, if any, off the front of a
 *             path and choose the inode to begin the name lookup relative to.
 */

static int get_device(char *path, char **subpath, struct inode **node_store) {
    int i, slash = -1, colon = -1;
    for (i = 0; path[i] != '\0'; i ++) {
        if (path[i] == ':') { colon = i; break; }
        if (path[i] == '/') { slash = i; break; }
    }
    if (colon < 0 && slash != 0) {
      /* *
      * No colon before a slash, so no device name specified, and the slash isn't leading
      * or is also absent, so this is a relative path or just a bare filename. Start from
      * the current directory, and use the whole thing as the subpath.
      * */
        *subpath = path;
        return vfs_get_curdir(node_store); //把当前目录的inode存到node_store
    }
    if (colon > 0) {
        /* device:path - get root of device's filesystem */
        path[colon] = '\0';

        /* device:/path - skip slash, treat as device:path */
        while (path[++ colon] == '/');
        *subpath = path + colon;
        return vfs_get_root(path, node_store);
    }

    /* *
     * we have either /path or :path
     * /path is a path relative to the root of the "boot filesystem"
     * :path is a path relative to the root of the current filesystem
     * */
    int ret;
    if (*path == '/') {
        if ((ret = vfs_get_bootfs(node_store)) != 0) {
            return ret;
        }
    }
    else {
        assert(*path == ':');
        struct inode *node;
        if ((ret = vfs_get_curdir(&node)) != 0) {
            return ret;
        }
        /* The current directory may not be a device, so it must have a fs. */
        assert(node->in_fs != NULL);
        *node_store = fsop_get_root(node->in_fs);
        vop_ref_dec(node);
    }

    /* ///... or :/... */
    while (*(++ path) == '/');
    *subpath = path;
    return 0;
}

/*
 * vfs_lookup - get the inode according to the path filename
 */
int vfs_lookup(char *path, struct inode **node_store) {
    int ret;
    struct inode *node;
    if ((ret = get_device(path, &path, &node)) != 0) {
        return ret;
    }
    if (*path != '\0') {
        ret = vop_lookup(node, path, node_store);
        vop_ref_dec(node);
        return ret;
    }
    *node_store = node;
    return 0;
}

/*
 * vfs_lookup_parent - Name-to-vnode translation.
 *  (In BSD, both of these are subsumed by namei().)
 */
int vfs_lookup_parent(char *path, struct inode **node_store, char **endp){
    int ret;
    struct inode *node;
    if ((ret = get_device(path, &path, &node)) != 0) {
        return ret;
    }
    *endp = path;
    *node_store = node;
    return 0;
}
```

我们注意到，这个流程中，有大量以`vop`开头的函数，它们都通过一些宏和函数的转发，最后变成对`inode`结构体里的`inode_ops`结构体的“成员函数”（实际上是函数指针）的调用。对于SFS文件系统的`inode`来说，会变成对`sfs`文件系统的具体操作。

<span id="sfs文件系统层的处理流程"></span>

#### SFS文件系统层的处理流程

这里需要分析文件系统抽象层中没有彻底分析的`vop_lookup`函数到底做了啥。下面我们来看看。在`sfs_inode.c`中的`sfs_node_dirops`变量定义了“.vop_lookup = sfs_lookup”，所以我们重点分析`sfs_lookup`的实现。注意：在lab8中，为简化代码，`sfs_lookup`函数中并没有实现能够对多级目录进行查找的控制逻辑（在ucore_plus中有实现）。

``` text
/*
 * sfs_lookup - Parse path relative to the passed directory
 *              DIR, and hand back the inode for the file it
 *              refers to.
 */
static int
sfs_lookup(struct inode *node, char *path, struct inode **node_store) {
    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    assert(*path != '\0' && *path != '/');
    vop_ref_inc(node);
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    if (sin->din->type != SFS_TYPE_DIR) {
        vop_ref_dec(node);
        return -E_NOTDIR;
    }
    struct inode *subnode;
    int ret = sfs_lookup_once(sfs, sin, path, &subnode, NULL);

    vop_ref_dec(node);
    if (ret != 0) {
        return ret;
    }
    *node_store = subnode;
    return 0;
}
```

`sfs_lookup`有三个参数：`node`，`path`，`node_store`。其中`node`是根目录`“/”`所对应的`inode`节点；`path`是文件`sfs_filetest1`的绝对路径`/sfs_filetest1`而`node_store`是经过查找获得的`sfs_filetest1`所对应的`inode`节点。

`sfs_lookup`函数以`“/”`为分割符，从左至右逐一分解`path`获得各个子目录和最终文件对应的`inode`节点。在本例中是调用`sfs_lookup_once`查找以根目录下的文件`sfs_filetest1`所对应的`inode`节点。当无法分解`path`后，就意味着找到了`sfs_filetest1`对应的`inode`节点，就可顺利返回了。

``` text
/*
 * sfs_lookup_once - find inode corresponding the file name in DIR's sin inode 
 * @sfs:        sfs file system
 * @sin:        DIR sfs inode in memory
 * @name:       the file name in DIR
 * @node_store: the inode corresponding the file name in DIR
 * @slot:       the logical index of file entry
 */
static int
sfs_lookup_once(struct sfs_fs *sfs, struct sfs_inode *sin, const char *name, struct inode **node_store, int *slot) {
    int ret;
    uint32_t ino;
    lock_sin(sin);
    {   // find the NO. of disk block and logical index of file entry
        ret = sfs_dirent_search_nolock(sfs, sin, name, &ino, slot, NULL);
    }
    unlock_sin(sin);
    if (ret == 0) {
        // load the content of inode with the the NO. of disk block
        ret = sfs_load_inode(sfs, node_store, ino);
    }
    return ret;
}
```

当然这里讲得还比较简单，`sfs_lookup_once`将调用`sfs_dirent_search_nolock`函数来查找与路径名匹配的目录项，如果找到目录项，则根据目录项中记录的`inode`所处的数据块索引值找到路径名对应的SFS磁盘`inode`，并读入SFS磁盘`inode`对的内容，创建SFS内存`inode`。
