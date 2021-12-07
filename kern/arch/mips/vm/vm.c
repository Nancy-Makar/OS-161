#include <vm.h>
#include <page.h>
#include <types.h>
#include <spinlock.h>

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

struct lock *coremap_lock;
void vm_bootstrap(void){

}

int vm_fault(int faulttype, vaddr_t faultaddress){
    (void) faulttype;
    (void) faultaddress;
    return 0;
}

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages){
    if (is_initialized()) {
        vaddr_t ret = get_first_free_page(npages);
        KASSERT(ret != 0);
        return ret;
    } else {
        paddr_t pa;
        spinlock_acquire(&stealmem_lock);
        pa = ram_stealmem(npages);
        vaddr_t ret = PADDR_TO_KVADDR(pa);
        KASSERT(ret != 0);
    	spinlock_release(&stealmem_lock);
	    return ret;
    }
}

void free_kpages(vaddr_t addr) {
    if (is_initialized()) {
        free_page(addr);
    }
}

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown_all(void){
    //nothing
}
void vm_tlbshootdown(const struct tlbshootdown *t){
    (void) t;
}