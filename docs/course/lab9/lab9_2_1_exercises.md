> 来源：[练习](http://8.135.34.58/lab2026/_book/lab9/lab9_2_1_exercises.html)

<span id="练习"></span>

### 练习

本实验围绕页面置换与虚拟内存管理两条主线展开。 第一条主线是页面置换算法。学生在理解已有 FIFO 页面置换算法的基础上，实现 Clock 页面置换算法，使内核在物理内存不足时能够根据页面访问情况选择合适的页面进行换出。 第二条主线是虚拟内存映射。学生需要理解 VMA（Virtual Memory Area）的组织方式，并实现基于 VMA 的虚拟地址空间管理，包括虚拟地址区域分配、内存映射、取消映射以及缺页异常处理。

本实验依赖实验2/3/4/5/6/7/8。请使用你此前设计的提示词生成相应实验代码，并将生成结果填写到本实验代码中标有“lab2”/“lab3”/“lab4”/“lab5”/“lab6”/“lab7”/“lab8”注释的对应位置，确保代码能够正确编译。

<span id="功能模块一：clock-页面置换算法（设计提示词借助-ai-完成）"></span>

#### 功能模块一：Clock 页面置换算法（设计提示词借助 AI 完成）

要求实现的函数

``` text
static int _clock_init_mm(struct mm_struct *mm);(kern\mm\swap_clock.c)
static int _clock_map_swappable(struct mm_struct *mm,uintptr_t addr, struct Page *page,int swap_in);(kern\mm\swap_clock.c)
static int _clock_swap_out_victim(struct mm_struct *mm,struct Page **ptr_page, int in_tick);(kern\mm\swap_clock.c)
```

整个功能模块的具体功能：这个模块负责在已有页面置换框架基础上实现 **Clock 页面置换算法**。内核已经提供统一的 `swap_manager` 接口以及 FIFO 页面置换算法作为参考。Clock 算法需要维护一组当前可以被换出的页面，并利用页面的访问状态判断页面是否应该获得“第二次机会”。模块主要包含三个方面：（1）初始化 Clock 算法所需的数据结构；（2）将新进入可换出集合的页面登记到 Clock 管理结构中；（3）当系统需要换出页面时，根据访问标志进行扫描并选择 victim page。模块完成后，当物理内存不足触发 `swap_out()` 时，Clock 算法应该能够正确选择一个满足换出条件的页面。

整个功能模块需要满足的要求：Clock 管理结构必须在初始化时处于一致状态。新加入的可换出页面必须能够被 Clock 算法正确管理。页面访问标志应当用于实现“第二次机会”机制。当页面访问标志表明页面近期被访问过时，应清除该状态并继续寻找 victim。当找到符合换出条件的页面后，应将其从当前可换出集合中正确移除。不能返回一个已经被移除、已经失效或者不属于当前 `mm` 的页面。如果当前没有可换出的页面，应按照现有 `swap_manager` 接口约定返回失败。实现不能破坏原有 FIFO、swap 以及页面管理框架。

整个功能模块应该满足的对外的接口：本模块不需要新增公共 API，而是实现已有 `swap_manager` 所规定的接口。核心接口为：`init_mm()`，`map_swappable()`，`swap_out_victim()`。其中`init_mm()`：初始化当前地址空间对应的 Clock 管理状态；`map_swappable()`：将页面加入 Clock 管理范围；`swap_out_victim()`：选择一个应该被换出的页面。`swap_manager` 已经规定了这些函数的统一调用方式，因此学生不应修改接口定义。

整个功能模块可以借助的接口函数、结构体、数据结构为:`struct mm_struct`,`struct Page`,`struct swap_manager`,`list_entry_t`,`mm->sm_priv`,`page->pra_page_link`,`page->visited`,`list_init()`,`list_add()`,`list_del()`,`list_next()`,`le2page()`.`swap_out()`

<span id="功能模块二：虚拟内存映射功能-（设计提示词借助-ai-完成）"></span>

#### 功能模块二：虚拟内存映射功能 （设计提示词借助 AI 完成）

要求实现的函数

``` text
uintptr_t do_mmap(struct mm_struct *mm,
                  uintptr_t addr,
                  size_t len, 
                  uint32_t vm_flags, 
                  struct file *file, 
                  off_t offset);(kern\mm\vmm.c)
```

整个功能模块的具体功能：`do_mmap` 是内核实现内存映射的核心函数。用户进程通过 `mmap` 请求一段虚拟地址空间后，系统调用层会将请求转换为内核所使用的 VMA 属性，并进一步调用 `do_mmap`。因此，本实验中不要求同学修改系统调用入口，而是重点完成 `do_mmap` 对虚拟地址空间和 VMA 的管理。

整个功能模块需要满足的要求：`len` 表示需要映射的虚拟地址空间大小，处理时需要考虑页边界对齐。映射区域必须位于合法的用户虚拟地址空间范围内。新创建的 VMA 不能与已有 VMA 发生重叠。当 `addr == 0` 时，需要在当前进程已有 VMA 之间寻找能够容纳映射区域的空闲空间。寻找空闲空间时，需要遍历所有可能的空闲区域，而不能找到第一个满足条件的区域后立即结束。空闲区域选择应遵循 **Best-Fit（最佳适配）策略**，即选择能够满足请求且剩余空间最小的区域。找到合适的虚拟地址后，需要创建并初始化 `vma_struct`。VMA 至少需要正确维护：`vm_start`,`vm_end`,`vm_flags`,`vm_pgoff`,`vm_file`。对于匿名映射，`vm_file` 应保持为空。对于文件映射，需要正确保存文件指针以及文件中的页偏移信息。文件映射涉及文件引用计数，不能在建立映射后丢失对文件对象的引用。创建完成的 VMA 必须加入当前进程的 VMA 链表，并正确维护 VMA 数量。如果执行过程中发生错误，需要避免留下不完整的 VMA 或错误的链表状态。`do_mmap` 不应该直接修改与本实验无关的系统调用接口和底层页表机制。不需要在 `do_mmap` 中一次性为整个映射区域分配物理页面。实验框架中的 `mmap` 同时支持匿名映射和文件映射；`MAP_SHARED`、`MAP_PRIVATE`、`MAP_ANONYMOUS` 等标志已经由实验框架定义。

整个功能模块可以借助的接口函数、结构体、数据结构为:`struct mm_struct`,`struct vma_struct`,`mm->mmap_list`,`mm->map_count`,`vma_create()`,`insert_vma_struct()`,`find_vma()`,`list_entry_t`,`list_entry()`,`list_next()`,`list_prev()`,`PGSIZE`,`USERTOP`,`USER_ACCESS`,`VM_READ`,`VM_WRITE`,`VM_EXEC`

<span id="功能模块三：虚拟内存取消映射功能（设计提示词借助-ai-完成）"></span>

#### 功能模块三：虚拟内存取消映射功能（设计提示词借助 AI 完成）

要求实现的函数：

``` text
int do_munmap(struct mm_struct *mm,
          uintptr_t addr,
          size_t len);(kern\mm\vmm.c)
```

整个功能模块的具体功能：`do_munmap` 用于解除进程虚拟地址空间中的一段内存映射。它与 `do_mmap` 构成完整的映射生命周期，`do_munmap` 的核心任务不是简单地“删除一个 VMA”，而是保证**虚拟地址空间、页表以及 VMA 管理结构之间的状态保持一致**。

整个功能模块的注意事项：`addr` 和 `len` 需要按照页边界进行处理。取消映射的区域必须位于合法的用户地址空间。需要检查指定区域是否确实存在对应的 VMA。不能因为部分区域存在 VMA 就错误地删除整个不匹配的 VMA。解除映射前需要正确处理对应的页表映射。如果该 VMA 是文件映射，需要正确处理文件引用计数。从 VMA 链表删除 VMA 后，需要释放 VMA 对象。删除 VMA 后，需要正确更新 `mm->map_count`。操作过程中不能破坏其他 VMA。如果参数非法或找不到合法映射区域，应按照实验框架的错误处理方式返回失败。`do_munmap` 不应该修改用户态 `munmap` 接口。不需要重新设计页表底层释放机制，应优先使用实验框架已经提供的接口。

整个功能模块可以借助的接口函数、结构体、数据结构：`struct mm_struct`,`struct vma_struct`,`mm->mmap_list`,`mm->map_count`,`find_vma()`,`unmap_range()`,``` vma_struct 的：``vm_start ```,`vm_end`,`vm_flags`,`vm_file`,`vm_pgoff`,`list_entry_t`,`list_entry()`,`list_del()`

<span id="扩展练习-challenge1：实现不考虑实现开销和效率的lru页替换算法（需要编程）"></span>

#### 扩展练习 Challenge1：实现不考虑实现开销和效率的LRU页替换算法（需要编程）

challenge部分不是必做部分，不过在正确最后会酌情加分。需写出有详细的设计、分析和测试的实验报告。完成出色的可获得适当加分。

<span id="扩展练习-challenge2：实现共享内存映射（mmap共享机制，需要编程）"></span>

#### 扩展练习 Challenge2：实现共享内存映射（mmap共享机制，需要编程）

在实际操作系统中，mmap 系统调用支持多种映射类型，其中共享映射（MAP_SHARED）和私有映射（MAP_PRIVATE）是最重要的两种。在之前的练习中，我们只实现了基本的匿名映射和文件映射，但缺少对共享机制的支持。

**共享映射（MAP_SHARED）** 允许多个进程映射同一文件区域，对映射区域的修改会反映到文件中，并且对其他映射同一文件的进程可见。 **私有映射（MAP_PRIVATE）** 则创建写时复制（Copy-On-Write）的映射，对映射区域的修改不会影响原始文件，也不会被其他进程看到。

在本练习中，你需要扩展原有的 do_mmap 和 do_munmap 函数，实现共享内存映射机制。具体任务包括：

- 修改VMA结构体：添加共享映射标志和支持写时复制的标志
- 扩展do_mmap函数：根据vm_flags中的MAP_SHARED/MAP_PRIVATE标志进行不同的处理
- 实现页面故障处理：修改缺页异常处理程序，支持写时复制（COW）机制
- 添加引用计数：为共享页面实现引用计数管理
- 修改do_munmap函数：正确处理共享页面的释放

<span id="练习"></span>

### 练习

本实验围绕页面置换与虚拟内存管理两条主线展开。 第一条主线是页面置换算法。学生在理解已有 FIFO 页面置换算法的基础上，实现 Clock 页面置换算法，使内核在物理内存不足时能够根据页面访问情况选择合适的页面进行换出。 第二条主线是虚拟内存映射。学生需要理解 VMA（Virtual Memory Area）的组织方式，并实现基于 VMA 的虚拟地址空间管理，包括虚拟地址区域分配、内存映射、取消映射以及缺页异常处理。

本实验依赖实验2/3/4/5/6/7/8。请使用你此前设计的提示词生成相应实验代码，并将生成结果填写到本实验代码中标有“lab2”/“lab3”/“lab4”/“lab5”/“lab6”/“lab7”/“lab8”注释的对应位置，确保代码能够正确编译。

<span id="功能模块一：clock-页面置换算法（设计提示词借助-ai-完成）"></span>

#### 功能模块一：Clock 页面置换算法（设计提示词借助 AI 完成）

要求实现的函数

``` text
static int _clock_init_mm(struct mm_struct *mm);(kern\mm\swap_clock.c)
static int _clock_map_swappable(struct mm_struct *mm,uintptr_t addr, struct Page *page,int swap_in);(kern\mm\swap_clock.c)
static int _clock_swap_out_victim(struct mm_struct *mm,struct Page **ptr_page, int in_tick);(kern\mm\swap_clock.c)
```

整个功能模块的具体功能：这个模块负责在已有页面置换框架基础上实现 **Clock 页面置换算法**。内核已经提供统一的 `swap_manager` 接口以及 FIFO 页面置换算法作为参考。Clock 算法需要维护一组当前可以被换出的页面，并利用页面的访问状态判断页面是否应该获得“第二次机会”。模块主要包含三个方面：（1）初始化 Clock 算法所需的数据结构；（2）将新进入可换出集合的页面登记到 Clock 管理结构中；（3）当系统需要换出页面时，根据访问标志进行扫描并选择 victim page。模块完成后，当物理内存不足触发 `swap_out()` 时，Clock 算法应该能够正确选择一个满足换出条件的页面。

整个功能模块需要满足的要求：Clock 管理结构必须在初始化时处于一致状态。新加入的可换出页面必须能够被 Clock 算法正确管理。页面访问标志应当用于实现“第二次机会”机制。当页面访问标志表明页面近期被访问过时，应清除该状态并继续寻找 victim。当找到符合换出条件的页面后，应将其从当前可换出集合中正确移除。不能返回一个已经被移除、已经失效或者不属于当前 `mm` 的页面。如果当前没有可换出的页面，应按照现有 `swap_manager` 接口约定返回失败。实现不能破坏原有 FIFO、swap 以及页面管理框架。

整个功能模块应该满足的对外的接口：本模块不需要新增公共 API，而是实现已有 `swap_manager` 所规定的接口。核心接口为：`init_mm()`，`map_swappable()`，`swap_out_victim()`。其中`init_mm()`：初始化当前地址空间对应的 Clock 管理状态；`map_swappable()`：将页面加入 Clock 管理范围；`swap_out_victim()`：选择一个应该被换出的页面。`swap_manager` 已经规定了这些函数的统一调用方式，因此学生不应修改接口定义。

整个功能模块可以借助的接口函数、结构体、数据结构为:`struct mm_struct`,`struct Page`,`struct swap_manager`,`list_entry_t`,`mm->sm_priv`,`page->pra_page_link`,`page->visited`,`list_init()`,`list_add()`,`list_del()`,`list_next()`,`le2page()`.`swap_out()`

<span id="功能模块二：虚拟内存映射功能-（设计提示词借助-ai-完成）"></span>

#### 功能模块二：虚拟内存映射功能 （设计提示词借助 AI 完成）

要求实现的函数

``` text
uintptr_t do_mmap(struct mm_struct *mm,
                  uintptr_t addr,
                  size_t len, 
                  uint32_t vm_flags, 
                  struct file *file, 
                  off_t offset);(kern\mm\vmm.c)
```

整个功能模块的具体功能：`do_mmap` 是内核实现内存映射的核心函数。用户进程通过 `mmap` 请求一段虚拟地址空间后，系统调用层会将请求转换为内核所使用的 VMA 属性，并进一步调用 `do_mmap`。因此，本实验中不要求同学修改系统调用入口，而是重点完成 `do_mmap` 对虚拟地址空间和 VMA 的管理。

整个功能模块需要满足的要求：`len` 表示需要映射的虚拟地址空间大小，处理时需要考虑页边界对齐。映射区域必须位于合法的用户虚拟地址空间范围内。新创建的 VMA 不能与已有 VMA 发生重叠。当 `addr == 0` 时，需要在当前进程已有 VMA 之间寻找能够容纳映射区域的空闲空间。寻找空闲空间时，需要遍历所有可能的空闲区域，而不能找到第一个满足条件的区域后立即结束。空闲区域选择应遵循 **Best-Fit（最佳适配）策略**，即选择能够满足请求且剩余空间最小的区域。找到合适的虚拟地址后，需要创建并初始化 `vma_struct`。VMA 至少需要正确维护：`vm_start`,`vm_end`,`vm_flags`,`vm_pgoff`,`vm_file`。对于匿名映射，`vm_file` 应保持为空。对于文件映射，需要正确保存文件指针以及文件中的页偏移信息。文件映射涉及文件引用计数，不能在建立映射后丢失对文件对象的引用。创建完成的 VMA 必须加入当前进程的 VMA 链表，并正确维护 VMA 数量。如果执行过程中发生错误，需要避免留下不完整的 VMA 或错误的链表状态。`do_mmap` 不应该直接修改与本实验无关的系统调用接口和底层页表机制。不需要在 `do_mmap` 中一次性为整个映射区域分配物理页面。实验框架中的 `mmap` 同时支持匿名映射和文件映射；`MAP_SHARED`、`MAP_PRIVATE`、`MAP_ANONYMOUS` 等标志已经由实验框架定义。

整个功能模块可以借助的接口函数、结构体、数据结构为:`struct mm_struct`,`struct vma_struct`,`mm->mmap_list`,`mm->map_count`,`vma_create()`,`insert_vma_struct()`,`find_vma()`,`list_entry_t`,`list_entry()`,`list_next()`,`list_prev()`,`PGSIZE`,`USERTOP`,`USER_ACCESS`,`VM_READ`,`VM_WRITE`,`VM_EXEC`

<span id="功能模块三：虚拟内存取消映射功能（设计提示词借助-ai-完成）"></span>

#### 功能模块三：虚拟内存取消映射功能（设计提示词借助 AI 完成）

要求实现的函数：

``` text
int do_munmap(struct mm_struct *mm,
          uintptr_t addr,
          size_t len);(kern\mm\vmm.c)
```

整个功能模块的具体功能：`do_munmap` 用于解除进程虚拟地址空间中的一段内存映射。它与 `do_mmap` 构成完整的映射生命周期，`do_munmap` 的核心任务不是简单地“删除一个 VMA”，而是保证**虚拟地址空间、页表以及 VMA 管理结构之间的状态保持一致**。

整个功能模块的注意事项：`addr` 和 `len` 需要按照页边界进行处理。取消映射的区域必须位于合法的用户地址空间。需要检查指定区域是否确实存在对应的 VMA。不能因为部分区域存在 VMA 就错误地删除整个不匹配的 VMA。解除映射前需要正确处理对应的页表映射。如果该 VMA 是文件映射，需要正确处理文件引用计数。从 VMA 链表删除 VMA 后，需要释放 VMA 对象。删除 VMA 后，需要正确更新 `mm->map_count`。操作过程中不能破坏其他 VMA。如果参数非法或找不到合法映射区域，应按照实验框架的错误处理方式返回失败。`do_munmap` 不应该修改用户态 `munmap` 接口。不需要重新设计页表底层释放机制，应优先使用实验框架已经提供的接口。

整个功能模块可以借助的接口函数、结构体、数据结构：`struct mm_struct`,`struct vma_struct`,`mm->mmap_list`,`mm->map_count`,`find_vma()`,`unmap_range()`,``` vma_struct 的：``vm_start ```,`vm_end`,`vm_flags`,`vm_file`,`vm_pgoff`,`list_entry_t`,`list_entry()`,`list_del()` 根据这个规范 E:\vx\xwechat_files\wxid_mefss7puk13f12_2675\msg\file\2026-09\注入式评分测试规范(1).zip 注释用英文，格式就参考这个，格式尽量保持一致，注意（1）框架代码里面有些文件中本身就包含check_xx函数，所以check函数涵盖到的检查我们不需要再test （2）如果是框架中已经给出的函数，inject可以注入到任何地方；函数对于我们需要自己实现的函数，inject可能只适合注入到函数开头（函数末尾如果有return语句，放在末尾就检测不到） （3）跟第二点类似，inject在定位函数时，需要谨慎一点，要确保匹配到的字符串在学生们自己实现的代码中有 根据这些内容完成lab9的测试，别的不要修改，就先完成测试文件，确保练习中要求的点全部能够测试到，并且是根据规范中要求的
