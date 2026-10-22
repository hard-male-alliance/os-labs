#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

#define BUDDY_MAX_ORDER 15

/* Free an allocation with its original request size, not a subrange. */
extern const struct pmm_manager buddy_pmm_manager;

#endif
