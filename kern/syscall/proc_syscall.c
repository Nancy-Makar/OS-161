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
#include <mips/trapframe.h>
#include <copyinout.h>
#include <lib.h>
#include <spl.h>
#include <limits.h>
#include <kern/wait.h>

struct lock *forklock;

int sys_fork(struct trapframe *tf,  pid_t *pid) {

    struct proc *p;
    struct addrspace *as;
    struct addrspace *newas;
    struct trapframe *newtf = kmalloc(sizeof(struct trapframe));
    struct proc_arg *new_proc_arg = kmalloc(sizeof(struct proc_arg));
    int err;

    if(forklock == NULL){ //If a lock does ont exist, create a new lock
        forklock = lock_create("fork_lock");
    }

    lock_acquire(forklock);
   

    p = curthread->t_proc;
    as = p->p_addrspace;

    //copy the address space
    as_copy(as, &newas);

    //create a new child procees + copy the filetable and working directory of the current parent process
    struct proc *child_proc = proc_fork("child process");

    child_proc->pid = get_next_pid();
    *pid = child_proc->pid;
    
    //copy and tweak the trapframe
    memcpy(newtf, tf, sizeof(struct trapframe));

    new_proc_arg->as = newas;
    new_proc_arg->tf = newtf;

    newtf->tf_v0 = 0;
    newtf->tf_a3 = 0;
    newtf->tf_epc += 4; //incerement epc so the child process does not keep forking

    //copy kernel thread
    err = thread_fork("process fork", child_proc, enter_forked_process, new_proc_arg, 0);
    if(err){
        lock_release(forklock);
        return err;
    }
    curproc->children[(int)*pid] = 1;
    lock_release(forklock);
    return 0;
}

int sys_execv(const_userptr_t program, char **args) {
    NO_WAIT = 1;
    //PART 0: copy in the program name
    char kernel_program[PATH_MAX];
    int err = 0;
    size_t len;
    err = copyinstr(program, kernel_program, PATH_MAX, &len);
    if (err || kernel_program == NULL) {
        //free up stuff
        return err;
    }
    if(len == 1){ //checks if program name is NULL
        return EISDIR;
    }

    //PART 1: DO THE ARGUMENTS...Count the number of arguments in the passed in array
    int count = 0;

    char *arg = NULL; 
    do {
        err = copyin((const_userptr_t) &args[count], &arg, sizeof(char *));
        if(err) return err;
        count++;
    } while (arg);
    count--;

    //Kernel_args is an array with count arguments, a program name in the first slot and a null terminated ending
    char **kernel_args = kmalloc(sizeof(char **) * (count + 1));
    if (kernel_args == NULL) return -1;  //some error

    int remaining_bytes = ARG_MAX;

    for (int i = 0; i < count; i++)  {
        if ((intptr_t)args[i] == (intptr_t)0x40000000){ // checks if the address of any of arg is invalid
            for (int j = 0; j < i; j++) {
                kfree(kernel_args[j]);
            }
            kfree(kernel_args);
            return EFAULT;
        }
        //size of string plus null terminator
        int size = (strlen(args[i]) + 1) * (sizeof(char));

        //Too many arguments
        if (remaining_bytes - size < 0) {
            for (int j = 0; j < i; j++) {
                kfree(kernel_args[j]);
            }
            kfree(kernel_args);
            return E2BIG;
        }

        kernel_args[i] = kmalloc(size);
        if (kernel_args[i] == NULL) {
            for (int j = 0; j <= i; j++) {
                kfree(kernel_args[j]);
            }
            kfree(kernel_args);
            return EFAULT;
        }
        err = copyinstr((const_userptr_t) args[i], kernel_args[i], size, NULL);
        if (err) {
            for (int j = 0; j <= i; j++) {
                kfree(kernel_args[j]);
            }
            kfree(kernel_args);
            return err;
        }
    }

    kernel_args[count] = NULL;
    
    //PART 3: create new address space
    struct addrspace *newaddr = as_create();
    if(newaddr == NULL){ //Insufficient virtual memory is available.
        for (int j = 0; j < count; j++) {
            kfree(kernel_args[j]);
        }
        kfree(kernel_args);
        return ENOMEM; 
    }
    struct addrspace *oldaddr = proc_setas(newaddr);

    //PART 4: load executable
    struct vnode *exc_v;
    err = vfs_open(kernel_program, O_RDONLY, 0, &exc_v);
    if (err) {
        for (int j = 0; j < count; j++) {
            kfree(kernel_args[j]);
        }
        kfree(kernel_args);
        return err;
    }

    vaddr_t entrypoint;
    err = load_elf(exc_v, &entrypoint);
    if (err) {
        for (int j = 0; j < count; j++) {
            kfree(kernel_args[j]);
        }
        kfree(kernel_args);
        return err;
    }

    //PART 5: define a new stack region for us to copy our arguments to
    vaddr_t stackptr;
    err = as_define_stack(newaddr, &stackptr);
    if (err) {
        for (int j = 0; j < count; j++) {
            kfree(kernel_args[j]);
        }
        kfree(kernel_args);
        return err;
    }

    //PART 6: Copy arguments to new address space
    //Create an array structure to hold the addresses of the arguments (plus null)
    vaddr_t *arg_addresses = (vaddr_t *)kmalloc((count + 1) *sizeof(vaddr_t));
    if (arg_addresses == NULL) {
        for (int j = 0; j < count; j++) {
            kfree(kernel_args[j]);
        }
        kfree(kernel_args);
        kfree(arg_addresses);
        return err;
    }

    //Move backwards, top of stack will have last argument
    for (int i = count - 1; i >= 0; i--) {

        //Need to ensure our arguments are aligned to 4 bytes (size of vaddr_t)
        size_t arg_len = strlen(kernel_args[i]) + 1;
        size_t arg_len_adj = ((arg_len + sizeof(vaddr_t) - 1) / sizeof(vaddr_t)) * sizeof(vaddr_t);
        //Adjust our stack pointer down by the length of our argument
        stackptr -= (arg_len_adj * sizeof(char));
        arg_addresses[i] = stackptr;

        err = copyoutstr((void*) kernel_args[i], (userptr_t) stackptr, arg_len, NULL);
        if (err) {
            for (int j = 0; j < count; j++) {
                kfree(kernel_args[j]);
            }
            kfree(kernel_args);
            kfree(arg_addresses);
            return err;
        }
    }
    //Now, put our pointers onto the stack
    for (int i = count; i >= 0; i--) {
        stackptr -= sizeof(vaddr_t);
        err = copyout((void*) &arg_addresses[i], (userptr_t) stackptr, sizeof(vaddr_t));
        if (err) {
            for (int j = 0; j < count; j++) {
                kfree(kernel_args[j]);
            }
            kfree(kernel_args);
            kfree(arg_addresses);
            return err;
        }
    }
    //PART 7: Clean up the old address space and structures
    as_destroy(oldaddr);
    for (int j = 0; j <= count; j++) {
        kfree(kernel_args[j]);
    }
    kfree(kernel_args);
    //PART 8: Enter the new process. 
    userptr_t argv = (userptr_t) stackptr;
    enter_new_process(count, argv, NULL, stackptr, entrypoint);
}

void sys_getpid(pid_t *pid)
{
    *pid = curproc->pid;
}

int sys_waitpid(pid_t pid, int *status, int options, pid_t *ret)
{
    int err;

    if (pid < 0) {
        return ENOSYS;
    }

    if(pid > PID_MAX){
        return ESRCH;
    }
    
    if (options != 0)
    {
        return EINVAL;
    }
    if (!curproc->children[pid] && curproc->pid != 0 && curproc->pid != 1)
    {
        return ECHILD;
    }
    if (get_pid(pid) == NULL)
    {
        return ESRCH;
    }
    
    //wait for the process corresponding to the pid number to exit
    err = pid_wait(pid, status);
    if (err) {
        return err;
    }
    //collect the status of the child process that the curthread has been waiting on
    *ret = get_pid(pid)->exitstatus;
    return 0;
}

int sys_exit(int exitcode, bool trap)
{
    int err = 0;
    pid_t pid = curproc->pid;

    //signal to all threads on the wait channel that are waiting for curthread to exit
    err = pid_exit(pid, exitcode, trap);
    if (err) {
        return err;
    }

    thread_exit(); //exit the current thread
    
    return 0; //We should never get here
}

// int sys_sbrk(intptr_t amount, vaddr_t *ret){
//     if(amount == 0){
//         ret = 
//     }
// }