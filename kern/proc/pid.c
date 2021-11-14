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

struct pid_table *table;
int pid_create(void) {
    table = kmalloc(sizeof(struct pid_table));
    KASSERT(table != NULL);
    for(int i = PID_MIN; i <= PID_MAX; i++){
        table->procs[i] = NULL;
    }
    table->pid_lock = lock_create("pid_table");
    return 0;
}

pid_t get_next_pid(void) {
    int err;
    if(table == NULL){
        err = pid_create();
        if(err){
            return -1; //TODO: proper error handling
        }
    }
    lock_acquire(table->pid_lock);
    for (int i = PID_MIN; i <= PID_MAX; i++)
    {
        if(table->procs[i] == NULL){ //check for the first bid that has a NULL process
            lock_release(table->pid_lock);
            return (pid_t)i;
        }
    }
    lock_release(table->pid_lock);
    return -1; //TODO: proper error handling
}



void add_proc(pid_t pid, struct proc *process){
    lock_acquire(table->pid_lock);
    table->procs[pid]= process;
    lock_release(table->pid_lock);
}

struct proc *get_proc(pid_t pid){
    lock_acquire(table->pid_lock);
    if((int)pid <= PID_MAX && (int)pid >= PID_MIN){
        if(table->procs[pid] != NULL){
            lock_release(table->pid_lock);
            return table->procs[pid]; 
        }      
    }

    lock_release(table->pid_lock);
    return NULL;
}


