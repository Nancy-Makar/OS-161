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
#include <spl.h>
#include <kern/wait.h>

struct lock *forklock;

int sys_fork(struct trapframe *tf,  pid_t *pid) {
    (void)pid;
    //int spl = splhigh();
    struct proc *p;
    struct addrspace *as;
    struct addrspace *newas;
    struct trapframe *newtf = kmalloc(sizeof(struct trapframe));
    struct proc_arg *new_proc_arg = kmalloc(sizeof(struct proc_arg));
    int err;

    if(forklock == NULL){
        forklock = lock_create("fork_lock");
    }

    lock_acquire(forklock);
   

    p = curthread->t_proc;
    as = p->p_addrspace;

    as_copy(as, &newas);

    struct proc *child_proc = proc_fork("child process");

    child_proc->pid = get_next_pid();

    add_proc(child_proc->pid, child_proc); //Add the process to the coressponding pid
    *pid = child_proc->pid;
    *newtf = *tf;

    new_proc_arg->as = newas;
    new_proc_arg->tf = newtf;

    newtf->tf_v0 = 0;
    newtf->tf_a3 = 0;
    newtf->tf_epc += 4;

    lock_release(forklock);

    err = thread_fork("process fork", child_proc, enter_forked_process, new_proc_arg, 0);
    if(err){  
       return err;
    }
    return 0;
}

void sys_getpid(pid_t *pid)
{
    *pid = curthread->t_proc->pid;
}

int sys_waitpid(pid_t pid, int *status, int options, pid_t *ret){
    (void) status;
    (void) options;
    struct proc *process = get_proc(pid);
    struct proc_table *process_entry = get_proc_table(pid);

    spinlock_acquire(&process->p_lock);
    if (&curthread->t_proc->exited)
    {
        //Do Something
        spinlock_release(&process->p_lock);
        return 1; //TODO: proper error handling
    }
    spinlock_release(&process->p_lock);

    lock_acquire(process_entry->proc_lk);
    cv_wait(process_entry->proc_cv, process_entry->proc_lk);
    lock_release(process_entry->proc_lk);
    *ret = pid;
    return 0;
}

int sys_exit(int exitcode){
    (void) exitcode;
    pid_t pid = curthread->t_proc->pid;

    lock_acquire(forklock);
    if (get_proc(pid) == NULL || curthread->t_proc->exited)
    {
        lock_release(forklock);
        int e = _MKWAIT_SIG(exitcode);
        
        return e;
        // other stuff
    }
    // struct proc_table *process_entry = get_proc_table(pid);
    // spinlock_acquire(&curthread->t_proc->p_lock);
    
    // cv_broadcast(process_entry->proc_cv, process_entry->proc_lk);
    
    remove_proc(pid);
    
    int e = _MKWAIT_EXIT(exitcode);
    (void) e;
    curthread->t_proc->exited = 1;
    lock_release(forklock);

    //spinlock_release(&curthread->t_proc->p_lock);
    thread_exit();
    return 0;
}