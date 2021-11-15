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
#include <limits.h>

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


    int spl = splhigh();  //during debugging, I found that a context switch happens during as_copy so I disabled interrupts
    as_copy(as, &newas);
    splx(spl);

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

int sys_execv(const char *program, char **args) {

    //PART 1: DO THE ARGUMENTS...Find the number of arguments in args
    int count = 0;
    int err = 0;
    do {
        count++;
        err = copyin((const_userptr_t) &args[i], (void *) arg, sizeof(char *));
    } while (arg != null);

    char **kernel_args = kmalloc(sizeof(char **) * i);
    if (kernel_args == NULL) return -1;  //some error

    int remaining_bytes = ARG_MAX;
    for (int i = 0; i < count; i++)  {
        //size of string plus null terminator
        int size = strlen(args[i]) + 1;

        //Too many arguments
        if (remaining_bytes - size < 0) {
            //Free all arguments
            for (int j = 0; j < i; j++) {
                kfree(kernel_args[j]);
            }
            kfree(kernel_args);
            return -1; //TODO: figure out what to return
        }

        kernel_args[i] = kmalloc(size);
        if (kernel_args[i] == NULL) {
            //Free all arguments
            for (int j = 0; j < i; j++) {
                kfree(kernel_args[j]);
            }
            kfree(kernel_args);
            return -1;  //TODO: figure out what to return
        }
        err = copyinstr(args[i], kernel_args[i], size, NULL);
        if (err) {
            //Free all arguments
            for (int j = 0; j < i; j++) {
                kfree(kernel_args[j]);
            }
            kfree(kernel_args);
            return err;
        }
    } 
    
    //PART 2: copy in the program name
    char *kernel_program;
    kernel_program = kmalloc(PATH_MAX);
    err = copyinstr(program, kernel_program, PATH_MAX, NULL);
    if (err || kernel_program == NULL) {
        //Free all arguments
        for (int j = 0; j < count; j++) {
            kfree(kernel_args[j]);
        }
        kfree(kernel_args);
        kfree(kernel_program);
        return -1;  //TODO: figure out what to return
    }

    //PART 3: create new address space
    struct addrspace *newaddr = as_create();
    struct addrspace *oldaddr = proc_setas(newaddr);

    //PART 4: load executable
    struct vnode *exc_v;
    err = vfs_open(kernel_program, O_RDONLY, &exc_v);
    if (err) {
        //free up stuff
        return -1;
    }

    vaddr_t *entrypoint;
    err = load_elf(exc_v, &entrypoint);
    if (err) {
        //free up stuff
        return -1;
    }

    //PART 5: define a new stack region
    vaddr_t *stackptr;
    as_define_stack(newaddr, &stackptr);
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

void sys_exit(int exitcode){
    (void) exitcode;
    pid_t pid = curthread->t_proc->pid;
    if(get_proc(pid) == NULL){
       // _MKWAIT_SIG(exitcode);
        //other stuff
    }
    struct proc_table *process_entry = get_proc_table(pid);
    spinlock_acquire(&curthread->t_proc->p_lock);
    curthread->t_proc->exited = 1;
    cv_broadcast(process_entry->proc_cv, process_entry->proc_lk);
    remove_pid(pid);
    //_MKWAIT_EXIT(exitcode);
    spinlock_release(&curthread->t_proc->p_lock);
    thread_exit();
}