#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <defs.h>

/* Single-hart, serialized callers only. Initialize after pmm_init(). */
void slub_init(void);
/* Returns uninitialized, 16-byte-aligned storage, or NULL for zero/OOM/overflow. */
void *kmalloc(size_t size);
/* Accepts NULL or an exact live kmalloc result. Empty slabs are returned to PMM. */
void kfree(void *ptr);
void slub_check(void);

#endif /* !__KERN_MM_SLUB_H__ */
