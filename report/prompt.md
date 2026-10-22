# Lab2 实际提示词记录

本文件区分用户原始请求、协调代理的实现指令和独立测试指令。四段格式 `[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]` 是实际开发中采用的规格表达，不声称用户一开始就提供了这些细节。

## 环境接入原始请求

```text
OK，把 lab2 解包并且接好吧
```

该阶段只解包教学骨架并接入构建、调试与评分工具，未实现物理内存初始化或分配器。

## 本次实验实现的用户原始请求

```text
那你去完成这个实验吧。你可以部署多智能体，指挥好他们。
```

用户授权协调代理完成实验并组织多智能体。以下为协调代理实际发给实现代理的后续规格复审提示词，按各模块的共享记录原文保存。这些不是用户原话，也不追溯声称为首次实现指令。独立测试提示词另行列出。

初始实现与独立测试任务的完整原文另见 [初始代理指令](prompt-initial-delegations.md)，与下列后续规格复审指令共同构成实际代码生成记录。

## 物理内存初始化与 Best-Fit

来源：`docs/lab2-memory-notes.md`，实现完成过程中的规格复审指令。

```text
[PROMPT] Review actual pmm.c and best_fit_pmm.c implementations; preserve provided tests and API. [RELY] dtb getters; PHYSICAL_MEMORY_OFFSET/END; nbase indexes DRAM_BASE; pmm_manager contract; course local documents. [GUARANTEE] page_init and complete best_fit_pmm_manager plus active selection. [SPECIFICATION] page_init validates nonzero supported DRAM range and overflow, page-aligns end placement, reserves firmware/kernel/metadata, frees only complete available pages, leaves no out-of-range descriptors; Best-Fit selects smallest sufficient block, exact n-page accounting, address-ordered insertion and two-sided coalescence, no supplied-test edits. Pre: boot DTB parsed, exclusive single-hart PMM. Post: byte layout and external API unchanged, focused objects compile. Document any remaining hidden risks. Parent handles integration.
```

规格设计说明：把页号上界与页数量区分开；用明确的保留区和地址边界约束初始化；把 Best-Fit 的选择、分裂、合并与计数都写成行为要求，而不是只说“实现最佳适应”。框架两个测试函数不允许改变。

实际问题：初次构建发现课程内核没有 `MIN` 宏，采用局部条件表达式修正；复审没有发现需要再次改动的缺陷。局部宿主模型检查和整机验证分别记录，不混淆证据层次。

## Buddy System

### 实现提示词

来源：`docs/lab2-buddy-notes.md`，实际最终复审与完善指令。

```text
[PROMPT] Review and finish buddy_pmm.c/h, preserve existing Page/API. [RELY] pmm_manager, Page flags/property/list link, physical PFNs, PGSIZE, exclusive reserved input span. [GUARANTEE] buddy_pmm_manager init/init_memmap/alloc_pages/free_pages/nr_free_pages/check. [SPECIFICATION] Pre: single hart and reserved in-range pages on init_memmap; partition arbitrary spans into maximal physically aligned 2^order blocks. alloc(n) returns rounded power-of-two block or NULL without state change; free(base,original_n) validates allocation and returns exactly rounded capacity, recursively merges only true same-order free buddies within registered memory. Reject zero/overflow requests safely. Preserve Page layout and independent Best-Fit ownership. Post: non-overlap, free count conservation, full reclamation. Include tests and limitations in note, no concurrency claims.
```

规格设计说明：强制采用绝对物理页号计算伙伴；任意起点分解为对齐块；释放必须匹配原始请求，不能沿用 Best-Fit 的子区间释放测试。失败后计数和已有分配不变。

实际复审补充：明确拒绝 0 与溢出大小的申请，并加入对应内建检查。非法释放在修改状态之前断言失败。

### 独立测试提示词

来源：`docs/lab2-test-plan.md`，实际发给独立验证代理的测试生成指令。

```text
[PROMPT] Generate independent Buddy tests in allocator_checks.c. [RELY] buddy_pmm_manager rounds n to power2; free original n; isolated reserved physical-PFN-aligned arena. [GUARANTEE] Buddy test invoked from allocator_checks. [SPECIFICATION] Assert alignment/nonoverlap, split/coalesce, rounded capacity accounting, exhaustion/zero/overflow, deterministic mixed trace and full baseline restoration; never mutate original supplied tests or leave global manager changed.
```

## SLUB

### 实现提示词

来源：`docs/lab2-slub-notes.md`，实际后续规格指令。

```text
[PROMPT] Finish simplified SLUB object allocator with two-level page/object management. [RELY] existing alloc_pages/free_pages and page/address mapping; single hart; no Page layout changes. [GUARANTEE] slub_init, kmalloc(size), kfree(ptr), slub_check. [SPECIFICATION] Pre: PMM initialized, slub_init once before allocations. kmalloc(0)/impossible sizes return NULL; all successes at least16-byte aligned and nonoverlapping with requested-byte payload safe; size classes cover small requests and page-backed large fallback. kfree(NULL) no-op; freeing valid live allocation restores ownership and empty slab returns all backing pages. Allocation failure preserves existing live allocations. Keep bounded metadata and explicit misuse assertions; no multicore/per-CPU performance claims.
```

规格设计说明：将“任意大小”解释为小对象大小类加大对象页面回退，而不是支持无限可用容量；明确 16 字节对齐、有效载荷完整安全、空 slab 回收以及 OOM 时活对象不受影响。串行前提与简化范围显式记录。

### 独立测试提示词

来源：`docs/lab2-test-plan.md`，实际发给独立验证代理的测试生成指令。

```text
[PROMPT] Generate independent SLUB tests. [RELY] slub_init/kmalloc/kfree and current PMM; 16-byte alignment. [GUARANTEE] SLUB test invoked from allocator_checks. [SPECIFICATION] Boundary sizes including0,16,17,2048,2049,4096,multipage; patterns across all requested payload bytes; nonalias of live objects; partial/full/empty transitions, deterministic mixed trace, overflow failure and exact PMM reclamation; kfree(NULL) no-op. Keep tests bounded and source-level independent of implementation metadata.
```

## 整机集成的实际后续迭代

以下是对实际修复的记录，不冒充未保存的用户提示词或四段代理指令。

1. 初始构建发现框架无 `MIN` 宏，改用局部条件表达式。
2. GDB 发现开启 `satp` 后下一条低地址取指无映射；先加入临时恒等映射、跳到高地址后再移除，整机检查验证最终映射。
3. 干净导出验证发现 Linux-GNU 回退工具链默认 PIC 下，分页前 `la` 经 GOT 取得高地址。将启动参数保存处两条伪指令改成显式 PC 相对的 `lla`，避免靠全局编译选项绕过问题；默认 ELF 工具链机器码不变。修复后干净导出的 Linux-GNU 构建通过四 RAM、17 GDB 断言和课程评分 20/20；未复用缓存、虚拟环境或既有构建产物。
