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

struct core_map **p_address;
void bootstrap(void){
    paddr_t p_size = ram_getsize();
    paddr_t p = ram_getfirstfree();

    size_t available_p = (p_size - p) / PAGE_SIZE;
    
}