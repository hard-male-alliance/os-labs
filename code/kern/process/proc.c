#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <kmonitor.h>

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:

  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  +
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : get the process's pid

*/

// the process set's list
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf) __attribute__((noreturn));
void switch_to(struct context *from, struct context *to);

//lab5 function2 2413077 2410824 2413174
static struct proc_struct *alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(*proc));
    if (proc != NULL)
    {
        memset(proc, 0, sizeof(*proc));
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->pgdir = boot_pgdir_pa;
    }
    return proc;
}

// set_proc_name - set the name of proc
char *
set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, strnlen(name, PROC_NAME_LEN));
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

static void link_child(struct proc_struct *parent, struct proc_struct *child)
{
    child->parent = parent;
    child->yptr = NULL;
    child->optr = parent->cptr;
    if (child->optr != NULL)
        child->optr->yptr = child;
    parent->cptr = child;
}

static void set_links(struct proc_struct *proc)
{
    list_add_before(&proc_list, &proc->list_link);
    link_child(current, proc);
    nr_process++;
}

// remove_links - clean the relation links of process
static void
remove_links(struct proc_struct *proc)
{
    list_del(&(proc->list_link));
    if (proc->optr != NULL)
    {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL)
    {
        proc->yptr->optr = proc->optr;
    }
    else
    {
        proc->parent->cptr = proc->optr;
    }
    nr_process--;
}

// get_pid - alloc a unique pid for process
static int
get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)
            {
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}






// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void
forkret(void)
{
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
static void
hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// unhash_proc - delete proc from proc hash_list
static void
unhash_proc(struct proc_struct *proc)
{
    list_del(&(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *
find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid)
            {
                return proc;
            }
        }
    }
    return NULL;
}

// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int
setup_kstack(struct proc_struct *proc)
{
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// setup_pgdir - alloc one page as PDT
static int
setup_pgdir(struct mm_struct *mm)
{
    struct Page *page;
    if ((page = alloc_page()) == NULL)
    {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    memcpy(pgdir, boot_pgdir_va, PGSIZE);

    mm->pgdir = pgdir;
    return 0;
}

// put_pgdir - free the memory space of PDT
static void
put_pgdir(struct mm_struct *mm)
{
    free_page(kva2page(mm->pgdir));
}

static struct mm_struct *new_mm(void)
{
    struct mm_struct *mm = mm_create();
    if (mm != NULL && setup_pgdir(mm) != 0)
    {
        mm_destroy(mm);
        return NULL;
    }
    return mm;
}

static void destroy_mm(struct mm_struct *mm)
{
    exit_mmap(mm);
    put_pgdir(mm);
    mm_destroy(mm);
}

static void use_mm(struct mm_struct *mm)
{
    struct mm_struct *old = current->mm;
    current->mm = mm;
    current->pgdir = mm != NULL ? PADDR(mm->pgdir) : boot_pgdir_pa;
    lsatp(current->pgdir);
    flush_tlb();
    if (old != NULL && mm_count_dec(old) == 0)
        destroy_mm(old);
}

//lab5 function3 2413077 2410824 2413174
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    struct mm_struct *src = current->mm, *mm = src;
    if (src == NULL)
        return 0;
    if (!(clone_flags & CLONE_VM))
    {
        mm = new_mm();
        if (mm == NULL)
            return -E_NO_MEM;
        lock_mm(src);
        int ret = dup_mmap(mm, src);
        unlock_mm(src);
        if (ret != 0)
        {
            destroy_mm(mm);
            return ret;
        }
    }
    mm_count_inc(mm);
    proc->mm = mm;
    proc->pgdir = PADDR(mm->pgdir);
    return 0;
}

static void copy_thread(struct proc_struct *proc, uintptr_t stack, struct trapframe *tf)
{
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    *proc->tf = *tf;
    proc->tf->gpr.a0 = 0;
    // Kernel threads leave the top frame free for a later kernel_execve.
    proc->tf->gpr.sp = trap_in_kernel(tf) ? (uintptr_t)proc->tf : stack;
    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)proc->tf;
}

//lab5 function3 2413077 2410824 2413174
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    if (nr_process >= MAX_PROCESS)
        return -E_NO_FREE_PROC;
    struct proc_struct *proc = alloc_proc();
    if (proc == NULL)
        return -E_NO_MEM;

    int ret = setup_kstack(proc);
    if (ret != 0)
        goto free_proc;
    ret = copy_mm(clone_flags, proc);
    if (ret != 0)
        goto free_stack;
    copy_thread(proc, stack, tf);

    bool intr_flag;
    local_intr_save(intr_flag);
    ret = proc->pid = get_pid();
    hash_proc(proc);
    set_links(proc);
    wakeup_proc(proc);
    local_intr_restore(intr_flag);
    return ret;

free_stack:
    put_kstack(proc);
free_proc:
    kfree(proc);
    return ret;
}

//lab5 function2 2413077 2410824 2413174
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    struct trapframe tf = {0};
    tf.gpr.s0 = (uintptr_t)fn;
    tf.gpr.s1 = (uintptr_t)arg;
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
    tf.epc = (uintptr_t)kernel_thread_entry;
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

static void wake_waiting_parent(struct proc_struct *parent)
{
    if (parent->wait_state == WT_CHILD)
        wakeup_proc(parent);
}

//lab5 function4 2413077 2410824 2413174
int do_exit(int error_code)
{
    if (current == idleproc || current == initproc)
        panic("cannot exit idle or init");

    bool intr_flag;
    local_intr_save(intr_flag);
    if (current->mm != NULL)
        use_mm(NULL);
    current->exit_code = error_code;
    current->state = PROC_ZOMBIE;
    wake_waiting_parent(current->parent);

    while (current->cptr != NULL)
    {
        struct proc_struct *child = current->cptr;
        current->cptr = child->optr;
        link_child(initproc, child);
        if (child->state == PROC_ZOMBIE)
            wake_waiting_parent(initproc);
    }
    schedule();
    panic("scheduled a zombie");
}

int do_yield(void)
{
    current->need_resched = true;
    return 0;
}

int do_kill(int pid)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    struct proc_struct *proc = find_proc(pid);
    int ret = -E_INVAL;
    if (proc != NULL && proc != initproc)
    {
        ret = -E_KILLED;
        if (!(proc->flags & PF_EXITING) && proc->state != PROC_ZOMBIE)
        {
            proc->flags |= PF_EXITING;
            if (proc->wait_state & WT_INTERRUPTED)
                wakeup_proc(proc);
            ret = 0;
        }
    }
    local_intr_restore(intr_flag);
    return ret;
}

static struct proc_struct *wait_target(int pid, bool *has_child)
{
    for (struct proc_struct *child = current->cptr; child != NULL; child = child->optr)
    {
        if (pid != 0 && pid != child->pid)
            continue;
        *has_child = true;
        if (child->state == PROC_ZOMBIE)
            return child;
    }
    return NULL;
}

//lab5 function2 2413077 2410824 2413174
int do_wait(int pid, int *code_store)
{
    if (code_store != NULL &&
        !user_mem_check(current->mm, (uintptr_t)code_store, sizeof(*code_store), true))
        return -E_INVAL;

    bool intr_flag;
    local_intr_save(intr_flag);
    struct proc_struct *child;
    for (;;)
    {
        bool has_child = false;
        child = wait_target(pid, &has_child);
        if (child != NULL)
            break;
        if (!has_child)
        {
            local_intr_restore(intr_flag);
            return -E_BAD_PROC;
        }
        current->state = PROC_SLEEPING;
        current->wait_state = WT_CHILD;
        schedule();
        if (current->flags & PF_EXITING)
            do_exit(-E_KILLED);
    }

    if (code_store != NULL &&
        !copy_to_user(current->mm, code_store, &child->exit_code, sizeof(*code_store)))
    {
        local_intr_restore(intr_flag);
        return -E_FAULT;
    }
    unhash_proc(child);
    remove_links(child);
    local_intr_restore(intr_flag);
    put_kstack(child);
    kfree(child);
    return 0;
}

static int load_segment(struct mm_struct *mm, const unsigned char *binary,
                        size_t size, const struct proghdr *ph)
{
    uintptr_t start = ph->p_va, end = start + ph->p_memsz;
    if (ph->p_filesz > ph->p_memsz || ph->p_offset > size ||
        ph->p_filesz > size - ph->p_offset || !USER_ACCESS(start, end) ||
        end > USTACKTOP - USTACKSIZE)
        return -E_INVAL_ELF;

    unsigned flags = 0, perm = PTE_U;
    if (ph->p_flags & ELF_PF_W)
    {
        flags |= VM_WRITE | VM_READ;
        perm |= PTE_W | PTE_R;
    }
    if (ph->p_flags & ELF_PF_R)
    {
        flags |= VM_READ;
        perm |= PTE_R;
    }
    if (ph->p_flags & ELF_PF_X)
    {
        flags |= VM_EXEC;
        perm |= PTE_X;
    }
    if (flags == 0)
        return -E_INVAL_ELF;

    int ret = mm_map(mm, start, ph->p_memsz, flags, NULL);
    if (ret != 0)
        return ret;

    uintptr_t file_end = start + ph->p_filesz;
    for (uintptr_t va = ROUNDDOWN(start, PGSIZE); va < end; va += PGSIZE)
    {
        struct Page *page = pgdir_alloc_page(mm->pgdir, va, perm);
        if (page == NULL)
            return -E_NO_MEM;
        unsigned char *dst = page2kva(page);
        memset(dst, 0, PGSIZE);
        uintptr_t lo = va > start ? va : start;
        uintptr_t hi = va + PGSIZE < file_end ? va + PGSIZE : file_end;
        if (lo < hi)
            memcpy(dst + (lo - va), binary + ph->p_offset + (lo - start), hi - lo);
    }
    return 0;
}

static int load_stack(struct mm_struct *mm)
{
    // The guard page isn't a VMA, so both CPU accesses and kernel copies reject it.
    uintptr_t start = USTACKTOP - USTACKSIZE + PGSIZE;
    int ret = mm_map(mm, start, USTACKTOP - start, VM_READ | VM_WRITE | VM_STACK, NULL);
    if (ret != 0)
        return ret;
    for (uintptr_t va = start; va < USTACKTOP; va += PGSIZE)
    {
        struct Page *page = pgdir_alloc_page(mm->pgdir, va, PTE_U | PTE_R | PTE_W);
        if (page == NULL)
            return -E_NO_MEM;
        memset(page2kva(page), 0, PGSIZE);
    }
    return 0;
}

static int load_segments(struct mm_struct *mm, const unsigned char *binary,
                         size_t size, const struct elfhdr *elf)
{
    bool executable_entry = false;
    for (size_t i = 0; i < elf->e_phnum; i++)
    {
        struct proghdr ph;
        memcpy(&ph, binary + elf->e_phoff + i * sizeof(ph), sizeof(ph));
        if (ph.p_type != ELF_PT_LOAD || (ph.p_memsz == 0 && ph.p_filesz == 0))
            continue;
        int ret = load_segment(mm, binary, size, &ph);
        if (ret != 0)
            return ret;
        if ((ph.p_flags & ELF_PF_X) && ph.p_va <= elf->e_entry &&
            elf->e_entry - ph.p_va < ph.p_memsz)
            executable_entry = true;
    }

    if (!executable_entry)
        return -E_INVAL_ELF;
    return load_stack(mm);
}

//lab5 function2 2413077 2410824 2413174
static int load_icode(const unsigned char *binary, size_t size)
{
    struct elfhdr elf;
    if (size < sizeof(elf))
        return -E_INVAL_ELF;
    memcpy(&elf, binary, sizeof(elf));
    if (elf.e_magic != ELF_MAGIC || elf.e_elf[0] != 2 || elf.e_elf[1] != 1 ||
        elf.e_type != 2 || elf.e_machine != 243 || elf.e_phentsize != sizeof(struct proghdr) ||
        elf.e_phoff > size || elf.e_phnum > (size - elf.e_phoff) / sizeof(struct proghdr))
        return -E_INVAL_ELF;

    struct mm_struct *mm = new_mm();
    if (mm == NULL)
        return -E_NO_MEM;
    int ret = load_segments(mm, binary, size, &elf);
    if (ret != 0)
    {
        destroy_mm(mm);
        return ret;
    }

    mm_count_inc(mm);
    use_mm(mm);
    asm volatile(".option push\n.option arch, +zifencei\nfence.i\n.option pop" ::: "memory");
    struct trapframe *tf = current->tf;
    memset(tf, 0, sizeof(*tf));
    tf->gpr.sp = USTACKTOP;
    tf->epc = elf.e_entry;
    tf->status = SSTATUS_SPIE | SSTATUS_SUM;
    return 0;
}

int do_execve(const char *name, size_t len, unsigned char *binary, size_t size)
{
    char local_name[PROC_NAME_LEN + 1] = {0};
    if (len > PROC_NAME_LEN)
        len = PROC_NAME_LEN;
    if ((len != 0 && !copy_from_user(current->mm, local_name, name, len, false)) ||
        !user_mem_check(current->mm, (uintptr_t)binary, size, false))
        return -E_INVAL;

    int ret = load_icode(binary, size);
    if (ret == 0)
        set_proc_name(current, local_name);
    return ret;
}

static int kernel_execve(const char *name, unsigned char *binary, size_t size)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    current->tf = (struct trapframe *)(current->kstack + KSTACKSIZE) - 1;
    int ret = do_execve(name, strlen(name), binary, size);
    if (ret == 0)
        forkrets(current->tf);
    local_intr_restore(intr_flag);
    return ret;
}

#ifndef TEST
#define TEST hello
#define TESTSTART _binary_obj___user_hello_out_start
#define TESTSIZE _binary_obj___user_hello_out_size
#endif

#define PROGRAM_NAME(x) #x
#define PROGRAM(x) PROGRAM_NAME(x)

static int user_main(void *arg)
{
    extern unsigned char TESTSTART[], TESTSIZE[];
    const char *name = PROGRAM(TEST);
    cprintf("kernel_execve: pid = %d, name = \"%s\".\n", current->pid, name);
    int ret = kernel_execve(name, TESTSTART, (size_t)TESTSIZE);
    cprintf("kernel_execve failed: %e\n", ret);
    return ret;
}

//lab5 function2 2413077 2410824 2413174
static int init_main(void *arg)
{
    size_t free_pages = nr_free_pages(), allocated = kallocated();
    int pid = kernel_thread(user_main, NULL, 0);
    if (pid < 0)
        panic("cannot start user program: %e", pid);
    int status;
    assert(do_wait(pid, &status) == 0);
    cprintf("user program exited with status %d\n", status);
    while (do_wait(0, NULL) == 0)
        ;
    assert(current->cptr == NULL && nr_process == 2);
    cprintf("all user-mode processes have quit.\n");
    assert(nr_free_pages() == free_pages && kallocated() == allocated);
    cprintf("init check memory pass.\n");
#ifdef DEBUG_GRADE
    kmonitor(NULL);
#endif
    for (;;)
        asm volatile("wfi");
}

void proc_init(void)
{
    list_init(&proc_list);
    for (size_t i = 0; i < HASH_LIST_SIZE; i++)
        list_init(&hash_list[i]);

    idleproc = alloc_proc();
    if (idleproc == NULL)
        panic("cannot allocate idleproc");
    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = true;
    set_proc_name(idleproc, "idle");
    nr_process++;
    current = idleproc;

    int pid = kernel_thread(init_main, NULL, 0);
    if (pid < 0)
        panic("cannot create initproc: %e", pid);
    initproc = find_proc(pid);
    set_proc_name(initproc, "init");
}

//LAB4 function3 2413077 2410824 2413174
void
proc_run(struct proc_struct *proc)
{
    struct proc_struct *prev = current;
    if (prev == proc)
        return;

    bool intr_flag;
    local_intr_save(intr_flag);
    current = proc;
    if (prev->pgdir != proc->pgdir)
    {
        write_csr(satp, ((uintptr_t)SATP_MODE_SV39 << 60) |
                        (proc->pgdir >> RISCV_PGSHIFT));
        flush_tlb();
    }
    switch_to(&prev->context, &proc->context);
    local_intr_restore(intr_flag);
}

void
cpu_idle(void)
{
    while (1)
    {
        if (current->need_resched)
            schedule();
    }
}
