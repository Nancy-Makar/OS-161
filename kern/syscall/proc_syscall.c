//implement as_copy
//use as_copy to copy the address space
//copy the trapframe
//copy the fd table
//

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <synch.h>
#include <current.h>
#include <copyinout.h>
#include <syscall.h>
#include <fd_table.h>
#include <vfs.h>
#include <vnode.h>
#include <proc.h>
#include <uio.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <proc_syscall.h>
#include <vm.h>
#include <addrspace.h>
#include<mips/trapframe.h>
#include <copyinout.h>
#include <lib.h>

int sys_fork(struct trapframe *tf,  pid_t *pid){
    struct proc *p;
    struct addrspace *as;
    struct fd_table *table;
    struct fd_table *new_fd_table;
    p = curthread->t_proc;
    as = p->p_addrspace;
    table = p->p_fdtable;
    struct addrspace *newas;
    as_copy(as, &newas);
    proc_setas(as);
    as_activate();

    struct trapframe *newtf = kmalloc(sizeof(struct trapframe));
    (void)pid;
    // (void)tf;
    // (void)newtf;
    memcpy(newtf, tf, sizeof(struct trapframe));
    newtf->tf_v0 = 0;

    
    fd_table_copy(table, &new_fd_table);


    //thread_fork()

    return 0;
}