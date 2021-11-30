#include <vm.h>

/* Initialization function */



void vm_bootstrap(void){

}

int vm_fault(int faulttype, vaddr_t faultaddress){
    (void) faulttype;
    (void) faultaddress;
    return 0;
}

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages){
    (void)npages;
    vaddr_t t = 0;
    return t;
}
void free_kpages(vaddr_t addr){
    (void) addr;
}

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown_all(void){
    //nothing
}
void vm_tlbshootdown(const struct tlbshootdown *t){
    (void) t;
}