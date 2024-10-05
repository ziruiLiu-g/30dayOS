#include "memory.h"
#include "io.h"

unsigned int memtest(unsigned int start, unsigned int end) {
    char flg486 = 0;
    unsigned int eflg, cr0, i;

    /* Check if CPU is 386 or 486 and above */
    eflg = io_load_eflags();
    eflg |= EFLAGS_AC_BIT;

    io_store_eflags(eflg);
    eflg = io_load_eflags();

    if ((eflg & EFLAGS_AC_BIT)) { // If cpu is 386, the AC-bit will be changed back to 0 automatically even if we manuanlly change it and load it to eflags
        flg486 = 1;
    }

    eflg &= ~EFLAGS_AC_BIT; // AC-BIT = 0
    io_store_eflags(eflg);

    if (flg486 != 0) {
        cr0 = io_load_cr0();
        cr0 |= CR0_CACHE_DISABLE; // Disable cache
        io_store_cr0(cr0);
    }

    i = memtest_sub(start, end);

    if (flg486 != 0) {
        cr0 = io_load_cr0();
        cr0 &= ~CR0_CACHE_DISABLE; // Enable cache
        io_store_cr0(cr0);
    }

    return i;
}

unsigned int memtest_sub(unsigned int start, unsigned int end) {
    unsigned int i, *p, old, pat0 = 0xaa55aa55, pat1 = 0x55aa55aa;
    for (i = start; i <= end; i += 0x1000) {
        p = (unsigned int *) (i + 0xffc);
        old = *p;   // save the old value
        *p = pat0;  // tryout
        *p ^= 0xffffffff; // reverse
        if (*p != pat1) {
            not_memory:
                *p = old;
                break;
        }

        *p ^= 0xffffffff;   // reverse
        if (*p != pat0) {   // check if the value has been recovered
            goto not_memory;
        }
        
        *p = old;   // recover value
    }

    return i;
}

void memman_init(struct MemMan *man) {
    man->frees = 0;
    man->maxfrees = 0;
    man->lostsize = 0;  // total mem size of failed to free mem
    man->losts = 0;
    return;
}

unsigned int memman_total(struct MemMan *man) {
    unsigned int i, t = 0;
    for (i = 0; i < man->frees; i++) {
        t += man->free[i].size;
    }

    return t;
}

unsigned int memman_alloc(struct MemMan *man, unsigned int size) {
    unsigned int i, a;
    for (i = 0; i < man->frees; i++) {
        if (man->free[i].size >= size) {
            a = man->free[i].addr;
            man->free[i].addr += size;
            man->free[i].size -= size;
            if (man->free[i].size == 0) {
                man->frees--;
                for (; i < man->frees; i++) {
                    man->free[i] = man->free[i + 1];
                }
            }
            return a;
        }
    }

    return 0;
}

unsigned int memman_alloc_4k(struct MemMan *man, unsigned int size) {
    unsigned int a;
    size = (size + 0xfff) & 0xfffff000;
    a = memman_alloc(man, size);
    return a;
}

int memman_free(struct MemMan *man, unsigned int addr, unsigned int size) {
    int i, j;

    /* Arrange the mem in addr order */
    for (i = 0; i < man->frees; i++) {
        if (man->free[i].addr > addr) {
            break;
        }
    }

    if (i > 0) {
        /* having available mem ahead */
        if (man->free[i - 1].addr + man->free[i - 1].size == addr) {
            man->free[i - 1].size += size;
            if (i < man->frees) {
                /* if available mem after */
                if (addr + size == man->free[i].addr) {
                    man->free[i - 1].size += man->free[i].size;
                    /* delete mem after */
                    man->frees--;
                    for (; i < man->frees; i++) {
                        man->free[i] = man->free[i + 1];
                    }
                }
            }
            return 0;
        }
    }

    /* no available ahead */
    if (i < man->frees) {
        /* if can merge with after mem */
        if (addr + size == man->free[i].addr) {
            man->free[i].addr = addr;
            man->free[i].size += size;
            return 0;
        }
    }

    /* no mem ahead and after could be merged */
    if (man->frees < MEMMAN_FREES) {
        for (j = man->frees; j > i; j--) {
            man->free[j] = man->free[j - 1];
        }
        man->frees++;
        if (man->maxfrees < man->frees) {
            man->maxfrees = man->frees;
        }
        man->free[i].addr = addr;
        man->free[i].size = size;
        return 0;
    }

    /* can't move back */
    man->losts++;
    man->lostsize += size;
    return -1;
}

int memman_free_4k(struct MemMan *man, unsigned int addr, unsigned int size) {
    int i;
    size = (size + 0xfff) & 0xfffff000;
    i = memman_free(man, addr, size);
    return i;
}