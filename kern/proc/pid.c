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
        table->pids[i]->process = NULL; //Set all process to NULL
    }
    table->pid_lock = lock_create("pid_table");
    return 0;
}

pid_t get_next_pid(void) {
    lock_acquire(table->pid_lock);
    for (int i = PID_MIN; i <= PID_MAX; i++)
    {
        if(table->pids[i]->process == NULL){ //check for the first bid that has a NULL process
            lock_release(table->pid_lock);
            return (pid_t)i;
        }
    }
    lock_release(table->pid_lock);
    return -1; //TODO: proper error handling
}

void remove_pid(pid_t pid) {
    lock_acquire(table->pid_lock);
    table->pids[pid]->process = NULL; //set the process coressponding to the pid to NULL
    lock_release(table->pid_lock);
}

void add_proc(pid_t pid, struct proc *process){
    table->pids[pid]->process = process;
}

struct proc *get_proc(pid_t pid){
    return table->pids[pid]->process;
}

