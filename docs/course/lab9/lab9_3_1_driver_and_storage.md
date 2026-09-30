> 来源：[页面置换后的驱动和存储](http://8.135.34.58/lab2026/_book/lab9/lab9_3_1_driver_and_storage.html)

<span id="页面置换后的驱动和存储"></span>

### 页面置换后的驱动和存储

在之前的文件系统章节实验中，我们使用 `make qemu` 启动内核，你是否思考过这样一个问题：这个虚拟计算机所使用的文件系统，它对应的“硬盘”究竟挂载在哪里？当页面置换发生时，**被换出的页面到底交换到何处去了**？实际上，我们并没有直接使用物理硬盘，而是通过 Ramdisk 在内存中模拟了一个块设备，用作文件系统和交换分区的存储后端。你可能会觉得这不过是把数据从一个内存区域搬到另一个内存区域，看似没有实际意义。然而，通过构建一个完整的分层设备模型，整个页面置换过程不再是一次随意的内存拷贝，而是一系列标准化的设备操作流程。这就像我们虽然用纸板搭建了一个建筑模型，但它的结构、比例和连接方式完全参照真实建筑的标准，让我们能够在有限的条件下理解复杂的系统架构。

> **扩展** 仔细思考一下，内存和硬盘，除了一个掉电后数据易失一个不易失，一个访问快一个访问慢，其实并没有本质的区别。 理论上，我们完全可以把一块机械硬盘加以改造，写好驱动之后，插到主板的内存插槽上作为内存条使用，当然我们要忽视性能方面的差距。

这个 Ramdisk 的物理数据来源于一个在编译阶段就预先准备好的磁盘镜像文件。让我们深入构建系统的细节，看看这个"虚拟硬盘"是如何诞生的。在 `Makefile` 中，我们使用 `dd` 命令创建一个全零的文件 `swap.img`，其大小可以通过 `bs`（块大小）和 `count`（块数量）参数精确控制。例如，`bs=4kB count=8` 会创建一个 32KB 的文件，这个大小足以容纳多个内存页面。

``` text
# Makefile (片段)
# 使用 dd 命令创建一个全零的 swap.img 文件，大小为 4KB * 8 = 32KB
$(SWAPIMG):
    $(V)dd if=/dev/zero of=$@ bs=4kB count=8
```

创建好镜像文件后，真正的魔法发生在链接阶段。通过 `ld` 命令的 `--format=binary` 参数，我们将 `swap.img` 的二进制内容直接嵌入到内核可执行文件中。这个操作使得镜像数据成为内核 `.data` 段的一部分，但又不占用编译时定义的静态数组空间。链接器会自动生成两个符号 `_binary_bin_swap_img_start` 和 `_binary_bin_swap_img_end`，它们分别指向这段嵌入数据在内核地址空间中的起始和结束位置。

``` text
# Makefile (链接阶段)
# 使用 --format=binary 将 swap.img 直接链接进内核可执行文件
$(kernel): $(KOBJS) $(SWAPIMG) $(SFSIMG)
    # ...
    $(V)$(LD) ... --format=binary $(SWAPIMG) ...
```

有了存储介质，下一步就是为其提供访问接口。Ramdisk 驱动的核心任务是将这段嵌入的内存区域伪装成一个支持标准扇区读写的设备。驱动通过引用链接器生成的符号来定位数据区域，并在初始化时将其绑定到设备对象中。这种设计使得驱动完全不知道数据的具体来源，它只关心**一个基地址和一段连续的内存区域**。

``` text
// Ramdisk 初始化：将内存地址与设备对象绑定
void ramdisk_init(int devno, struct ide_device *dev) {
    if (devno == SWAP_DEV_NO) {
        // 绑定数据源：让 iobase 指向 swap.img 的内存数据
        dev->iobase = (uintptr_t)_binary_bin_swap_img_start;

        // 计算容量：通过起止地址之差计算扇区数
        dev->size = (unsigned int)(_binary_bin_swap_img_end 
                                 - _binary_bin_swap_img_start) / SECTSIZE;

        // 绑定行为：注册具体的读写函数
        dev->read_secs = ramdisk_read;
        dev->write_secs = ramdisk_write;
        dev->valid = 1; // 标记设备可用
    }
}
```

Ramdisk的读写逻辑虽然核心仍然是内存拷贝，但加入了关键的边界检查机制。这种检查确保任何读写请求都不会超出分配给Ramdisk的内存区域范围，从而防止意外的内存越界访问导致内核数据被破坏。

``` text
// Ramdisk 写操作：带有边界检查的内存拷贝
static int ramdisk_write(struct ide_device *dev, size_t secno, const void *src,
                         size_t nsecs) {
    // 关键安全检查：防止写入超过镜像大小
    nsecs = MIN(nsecs, dev->size - secno);

    // 执行真正的"写入"：内存拷贝
    memcpy((void *)(dev->iobase + secno * SECTSIZE), src, nsecs * SECTSIZE);
    return 0;
}
```

仅仅实现驱动还不够，操作系统需要一套统一的模型来管理多种可能存在的块设备。为此，内核引入了 IDE 设备抽象层。该层定义了一个通用的设备结构体 `struct ide_device`，其中不仅包含设备容量、基地址等属性，更重要的是包含两个函数指针 `read_secs` 和 `write_secs`。这种设计使得具体的读写逻辑与上层接口解耦，任何符合此接口的驱动都能被无缝集成。

``` text
// IDE 设备抽象结构（关键字段）
struct ide_device {

    // 其他成员
    unsigned int size;   // 设备总扇区数
    uintptr_t iobase;    // 数据区基地址
    // 多态的关键：函数指针
    int (*read_secs)(struct ide_device *dev, size_t secno, void *dst, size_t nsecs);
    int (*write_secs)(struct ide_device *dev, size_t secno, const void *src, size_t nsecs);
};
```

> **扩展** 当前我们使用的是 Ramdisk，这只是一种在内存中模拟的简单设备。在实际的系统演进中，我们可以预见以下几种扩展场景：
>
> 当我们需要在虚拟化环境中获得更好的磁盘性能时，可以引入 Virtio 块设备驱动。Virtio 是一种半虚拟化标准，通过在宿主机和虚拟机之间定义清晰的接口，能够显著提升虚拟设备的 I/O 效率。实现时，我们只需编写一个符合 struct ide_device 接口的 Virtio 驱动，并将其注册到 IDE 抽象层中，上层所有代码（包括 SwapFS 和文件系统）就能无缝切换到更高效的虚拟磁盘。
>
> 更进一步，当系统需要部署到真实的物理硬件时，我们可以根据具体的硬件平台实现相应的驱动。无论是 SATA、NVMe 还是其他类型的块设备，只要按照 read_secs 和 write_secs 的接口要求实现具体的读写函数，并正确初始化设备结构体，就能将真实的物理硬盘接入到这个存储子系统中。这意味着整个页面置换机制和文件系统可以在不改动上层逻辑的情况下，从模拟环境迁移到真实硬件环境。
>
> 这种设计体现了操作系统开发中重要的抽象原则：通过定义清晰的接口，将硬件相关的具体实现与系统核心逻辑分离，使得系统既能在简单的教学环境中运行，又具备向生产环境演进的能力。

系统启动时，`ide_init` 函数负责将具体的驱动实现注册到 IDE 抽象层中。这里我们可以看到系统支持多种设备并存的设计：交换设备和文件系统设备虽然都是 Ramdisk，但被分配了不同的设备号，绑定到不同的内存区域。

``` text
// IDE 层初始化：注册具体设备
void ide_init(void) {
    // 初始化交换设备 (SWAP_DEV_NO = 1)
    ramdisk_init(SWAP_DEV_NO, &ide_devices[SWAP_DEV_NO]);
    // 初始化文件系统设备 (DISK0_DEV_NO = 2)
    ramdisk_init(DISK0_DEV_NO, &ide_devices[DISK0_DEV_NO]);
}
```

当上层需要读写磁盘时，调用的是统一的 `ide_write_secs` 函数。这个函数并不直接操作硬件，而是作为一个分发器，根据设备号找到对应的设备对象，然后通过其函数指针调用具体的实现。这种设计使得上层代码完全不用关心底层是 Ramdisk 还是未来可能添加的真实硬盘驱动。

``` text
// IDE 层的统一写接口：动态分发请求
int ide_write_secs(unsigned short ideno, uint32_t secno, const void *src,
                   size_t nsecs) {
    // 参数校验...
    // 动态分发：调用注册在具体设备中的函数
    return ide_devices[ideno].write_secs(&ide_devices[ideno], secno, src, nsecs);
}
```

对于页面置换算法而言，它并不直接与 IDE 层交互，而是通过一个称为 SwapFS 的中间层。SwapFS 的任务是处理内存页与磁盘扇区之间的转换。这里涉及一个关键的数据结构 `swap_entry_t`，它是一个 32 位整数，其中高 24 位表示在交换分区中的索引，低 8 位用于存储标志位。

``` text
// swap_offset 宏：从 swap_entry_t 中提取交换区索引
#define swap_offset(entry) ({                                       \
               size_t __offset = (entry >> 8);                      \
               if (!(__offset > 0 && __offset < max_swap_offset)) { \
                    panic("invalid swap_entry_t = %08x.\n", entry); \
               }                                                    \
               __offset;                                            \
          })
```

SwapFS 的写操作将页面数据写入交换设备。这里需要处理单位转换：一个内存页（通常 4KB）对应多个磁盘扇区（每个 512B）。通过 `PAGE_NSECT`（值为 8）的乘法运算，SwapFS 将页索引转换为起始扇区号，然后调用 IDE 层的统一接口。

``` text
// SwapFS 的写操作：将内存页写入交换设备
int swapfs_write(swap_entry_t entry, struct Page *page) {
    // 将页面的交换索引转换为起始扇区号
    // PAGE_NSECT 定义了每页所需的扇区数（通常为 8）
    return ide_write_secs(SWAP_DEV_NO, swap_offset(entry) * PAGE_NSECT,
                          page2kva(page), PAGE_NSECT);
}
```

至此，我们看到了一个完整的调用链条：当需要换出一个页面时，`Swap Manager` 调用 `swapfs_write`；SwapFS 将页面信息转换为磁盘扇区请求，调用 `ide_write_secs`；IDE 抽象层根据设备号找到已初始化的 Ramdisk 设备对象，通过其函数指针调用 `ramdisk_write`；最后，Ramdisk 驱动执行内存拷贝，将页面数据写入由链接器嵌入的那段镜像内存中。虽然底层操作依然是内存拷贝，但每一层都有明确的职责：`SwapFS` 负责**页与扇区的转换**，IDE层负责**设备抽象与多态**，Ramdisk负责**内存模拟磁盘区域的管理与保护**。这样的分层设计不仅让页面置换的功能得以实现，更展示了一个可扩展的存储子系统如何通过清晰的接口定义和职责分离来构建。如果未来需要支持真正的硬盘设备，我们只需要在驱动层实现相应的读写函数并注册到IDE抽象层，而SwapFS和页面置换算法完全无需改动。这正是操作系统设备管理架构的价值所在：通过适当的**抽象**，让上层功能与底层实现解耦，使系统既能在简单环境下运行演示，又具备向复杂环境演进的能力。
