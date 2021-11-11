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
#include <vm.h>
#include <addrspace.h>
#include<mips/trapframe.h>
#include <copyinout.h>
#include <lib.h>

int sys_fork(struct trapframe *tf,  pid_t *pid) {
    (void)pid;
    struct proc *p;
    struct addrspace *as;
    p = curthread->t_proc;
    as = p->p_addrspace;

    struct addrspace *newas;
    

    struct trapframe *newtf = kmalloc(sizeof(struct trapframe));
    //memcpy(newtf, tf, sizeof(struct trapframe));

    *newtf = *tf;
    newtf->tf_v0 = 0;
    newtf->tf_epc += 4;



    as_copy(as, &newas);

    struct proc_arg *new_proc_arg = kmalloc(sizeof(struct proc_arg));
    new_proc_arg->as = newas;
    new_proc_arg->tf = newtf;

    struct proc *child_proc = proc_fork("child process");
    child_proc->pid = get_next_pid();
    add_proc(child_proc->pid, child_proc); //Add the process to the coressponding pid
    *pid = child_proc->pid;

    thread_fork("process fork", child_proc, enter_forked_process, new_proc_arg, 0);

    return 0;
}


// pid_t get_pid() {
//     return curporc->pid;
// }

pid_t sys_waitpid(pid_t pid, int *status, int options){
    (void) status;
    (void) options;
    struct proc *process = get_proc(pid);
    (void) process;
    return pid;
}

void sys_exit(int exitcode){
    (void) exitcode;
    pid_t pid = curthread->t_proc->pid;
    if(get_proc(pid) == NULL){ 

    }
    remove_pid(pid);
    thread_exit();
}