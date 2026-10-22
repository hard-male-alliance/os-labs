#include <slub.h>
#include <assert.h>
#include <list.h>
#include <pmm.h>
#include <stdio.h>

#define SLUB_CLASSES 8
#define SLUB_MAGIC 0x534c55424c414232ULL
#define SLUB_ALIGN 16
#define SLUB_LARGE SLUB_CLASSES

struct slab {
    uint64_t magic;
    list_entry_t link;
    void *free;
    size_t npages;
    size_t class;
    size_t capacity;
    size_t used;
    uint64_t allocated[4];
};

#define SLUB_HEADER ROUNDUP(sizeof(struct slab), SLUB_ALIGN)

static list_entry_t partial[SLUB_CLASSES];
static bool initialized;

void
slub_init(void) {
    if (initialized) {
        return;
    }
    for (size_t i = 0; i < SLUB_CLASSES; i++) {
        list_init(&partial[i]);
    }
    initialized = 1;
}

static size_t
class_size(size_t class) {
    return (size_t)SLUB_ALIGN << class;
}

static void *
slab_data(struct slab *slab) {
    return (char *)slab + SLUB_HEADER;
}

static struct slab *
new_slab(size_t npages, size_t class) {
    struct Page *page = alloc_pages(npages);
    if (page == NULL) {
        return NULL;
    }
    struct slab *slab = (void *)(page2pa(page) + va_pa_offset);
    slab->magic = SLUB_MAGIC;
    list_init(&slab->link);
    slab->free = NULL;
    slab->npages = npages;
    slab->class = class;
    slab->capacity = slab->used = 0;
    for (size_t i = 0; i < 4; i++) {
        slab->allocated[i] = 0;
    }
    return slab;
}

static void
release_slab(struct slab *slab) {
    size_t npages = slab->npages;
    struct Page *page = pa2page((uintptr_t)slab - va_pa_offset);
    slab->magic = 0;
    free_pages(page, npages);
}

static struct slab *
new_small_slab(size_t class) {
    struct slab *slab = new_slab(1, class);
    if (slab == NULL) {
        return NULL;
    }
    size_t size = class_size(class);
    slab->capacity = (PGSIZE - SLUB_HEADER) / size;
    assert(slab->capacity > 0 && slab->capacity <= 256);
    /* Free objects carry their own next link; live object bytes belong to callers. */
    for (size_t i = slab->capacity; i > 0; i--) {
        void *obj = (char *)slab_data(slab) + (i - 1) * size;
        *(void **)obj = slab->free;
        slab->free = obj;
    }
    list_add(&partial[class], &slab->link);
    return slab;
}

static void *
alloc_large(size_t size) {
    size_t max = (size_t)-1;
    if (size > max - SLUB_HEADER - (PGSIZE - 1)) {
        return NULL;
    }
    size_t npages = (size + SLUB_HEADER + PGSIZE - 1) / PGSIZE;
    if (npages > nr_free_pages()) {
        return NULL;
    }
    struct slab *slab = new_slab(npages, SLUB_LARGE);
    if (slab == NULL) {
        return NULL;
    }
    slab->used = 1;
    return slab_data(slab);
}

void *
kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    slub_init();
    if (size > class_size(SLUB_CLASSES - 1)) {
        return alloc_large(size);
    }
    size_t class = 0;
    while (class_size(class) < size) {
        class++;
    }
    struct slab *slab;
    if (list_empty(&partial[class])) {
        slab = new_small_slab(class);
    } else {
        slab = to_struct(list_next(&partial[class]), struct slab, link);
    }
    if (slab == NULL) {
        return NULL;
    }
    void *obj = slab->free;
    size_t slot = ((uintptr_t)obj - (uintptr_t)slab_data(slab)) / class_size(class);
    uint64_t mask = 1ULL << (slot % 64);
    assert(!(slab->allocated[slot / 64] & mask));
    slab->allocated[slot / 64] |= mask;
    slab->free = *(void **)obj;
    slab->used++;
    if (slab->used == slab->capacity) {
        list_del_init(&slab->link);
    }
    return obj;
}

void
kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    struct slab *slab = (void *)ROUNDDOWN((uintptr_t)ptr, PGSIZE);
    assert(slab->magic == SLUB_MAGIC && slab->used > 0);
    if (slab->class == SLUB_LARGE) {
        assert(ptr == slab_data(slab) && slab->used == 1);
        release_slab(slab);
        return;
    }
    assert(slab->class < SLUB_CLASSES);
    size_t size = class_size(slab->class);
    uintptr_t offset = (uintptr_t)ptr - (uintptr_t)slab_data(slab);
    assert((uintptr_t)ptr >= (uintptr_t)slab_data(slab));
    assert(offset % size == 0 && offset / size < slab->capacity);
    size_t slot = offset / size;
    uint64_t mask = 1ULL << (slot % 64);
    assert(slab->allocated[slot / 64] & mask);
    slab->allocated[slot / 64] &= ~mask;
    if (slab->used == slab->capacity) {
        list_add(&partial[slab->class], &slab->link);
    }
    *(void **)ptr = slab->free;
    slab->free = ptr;
    slab->used--;
    if (slab->used == 0) {
        list_del_init(&slab->link);
        release_slab(slab);
    }
}

void
slub_check(void) {
    static void *objects[300];
    static const size_t sizes[] = {1, 16, 17, 31, 32, 33, 63, 64, 65,
        127, 128, 129, 255, 256, 257, 511, 512, 513, 1023, 1024,
        1025, 2047, 2048, 2049, 4095, 4096, 4097, 8192};
    size_t before = nr_free_pages();
    slub_init();
    assert(kmalloc(0) == NULL && kmalloc((size_t)-1) == NULL);
    kfree(NULL);
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        unsigned char *obj = kmalloc(sizes[i]);
        assert(obj != NULL && (uintptr_t)obj % SLUB_ALIGN == 0);
        for (size_t j = 0; j < sizes[i]; j++) {
            obj[j] = (unsigned char)(j + i);
        }
        for (size_t j = 0; j < sizes[i]; j++) {
            assert(obj[j] == (unsigned char)(j + i));
        }
        kfree(obj);
        assert(nr_free_pages() == before);
    }
    /* Cross a slab boundary, reopen full slabs, and preserve neighboring data. */
    for (size_t i = 0; i < 300; i++) {
        objects[i] = kmalloc(16);
        assert(objects[i] != NULL);
        *(size_t *)objects[i] = i;
    }
    for (size_t i = 0; i < 300; i += 2) {
        assert(*(size_t *)objects[i] == i);
        kfree(objects[i]);
        objects[i] = NULL;
    }
    for (size_t i = 0; i < 300; i += 2) {
        objects[i] = kmalloc(16);
        assert(objects[i] != NULL);
        *(size_t *)objects[i] = i;
    }
    for (size_t i = 300; i > 0; i--) {
        assert(*(size_t *)objects[i - 1] == i - 1);
        kfree(objects[i - 1]);
    }
    assert(nr_free_pages() == before);
    cprintf("slub_check() succeeded!\n");
}
