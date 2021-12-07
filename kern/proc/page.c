#include <types.h>
#include <fd_table.h>
#include <kern/errno.h>
#include <spl.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <vfs.h>
#include <synch.h>
#include <kern/fcntl.h>
#include <limits.h>
#include <kern/wait.h>
#include <copyinout.h>
#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <vm.h>
#include <page.h>
#include <kern/errno.h>
#include <lib.h>
#include <spinlock.h>
#include <mips/tlb.h>

struct coremap *coremap;
struct lock *coremap_lock;
paddr_t base_address;
bool coremap_initialized;
int num_pages;


void coremap_init(void) {

    paddr_t last_address = ram_getsize();
    base_address = ram_getfirstfree();

    //Number of page entries we will need for our coremap
    num_pages = (last_address-base_address)/PAGE_SIZE;

    //Each page will have a coremap entry
    size_t coremap_size = (num_pages *sizeof(struct coremap));

    //Align our coremap to the size of our pages
    coremap_size = ((coremap_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

    //Number of pages our coremap will take up
    int coremap_pages = coremap_size / PAGE_SIZE;

    //Our coremap_first struct will point to the first entry in our coremap
    coremap = (struct coremap*) PADDR_TO_KVADDR(base_address);
    
    //For the number of pages our coremap takes up, make those pages unavailable
    for (int i = 0; i < coremap_pages; i++) {
        coremap[i].in_use = true;
        coremap[i].is_first = false;
        coremap[i].count = 0;
    }

    //For the remaining pages, make those pages available
    for (int i = coremap_pages; i < num_pages; i++) {
        coremap[i].in_use = false;
    }
    coremap_lock = lock_create("coremap lock");
    coremap_initialized = true;
}

vaddr_t get_first_free_page(int npages) {
    int count = 0;

    lock_acquire(coremap_lock);

    //Find n continous pages
    for (int i = 0; i < num_pages; i++) {
        if (!coremap[i].in_use) {
            count++;

            //We've found our pages
            if (count == npages) {
                int start = (i - (npages - 1));
                coremap[start].count = npages;
                coremap[start].is_first = true;

                for (int j = start; j < start + npages; j++) {
                    coremap[j].in_use = true;
                }
                paddr_t free_paddr= start*PAGE_SIZE + base_address;
                lock_release(coremap_lock);
                return PADDR_TO_KVADDR(free_paddr);
            }
        } else {
            count = 0;
        }
    }
    //No free pages :(
    lock_release(coremap_lock);
    KASSERT(false);
    return 0;
}

void free_page(vaddr_t v_page_addr) {
    lock_acquire(coremap_lock);
    paddr_t p_page_addr = KVADDR_TO_PADDR(v_page_addr);
    int index = (p_page_addr - base_address) / PAGE_SIZE;
    int count = coremap[index].count;

    for (int i = index; i < index + count; i++) {
        coremap[i].in_use = false;
        coremap[i].count = 0;
        coremap[i].is_first = false;
    }

    lock_release(coremap_lock);
}

bool is_initialized(void) {
    return coremap_initialized;
}