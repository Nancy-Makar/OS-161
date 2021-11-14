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

    //lock_release(forklock);

    err = thread_fork("process fork", child_proc, enter_forked_process, new_proc_arg, 0);
    if(err){
        lock_release(forklock);
        return err;
    }
    lock_release(forklock);
    return 0;
}

void sys_getpid(pid_t *pid)
{
    *pid = curproc->pid;
}

int sys_waitpid(pid_t pid, int *status, int options, pid_t *ret)
{
    (void)pid;
    if (options != 0)
    {
        return EINVAL;
    }
    (void)status;
    struct proc *p = get_proc(pid);
    if (p != NULL)
    {
        spinlock_acquire(&p->p_lock);
        if (p->exited)
        {
            return 0;
        }
        spinlock_release(&p->p_lock);
        while (p->exited)
        {
            thread_yield();
        }
        *ret = *status; // will this work?
    }
    return 0;
}

    int sys_exit(int exitcode)
    {
        (void)exitcode;
        lock_acquire(forklock);
        pid_t pid = curproc->pid;
        if (curproc->exited || get_proc(pid) == NULL)
        {
            int e = _MKWAIT_SIG(exitcode);
            lock_release(forklock);
            return e;
        }
        curproc->exited = 1;
        int e = _MKWAIT_EXIT(exitcode);
        (void)e;
        //remove_proc(pid);
        

        thread_exit();
        lock_release(forklock);

        return 0;
    }