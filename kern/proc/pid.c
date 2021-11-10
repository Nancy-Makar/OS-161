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
    table->num_proc = 0;
    table->next_pid = 2;
    table->pid_lock = lock_create("pid_table");
    return 0;
}

pid_t get_next_pid(void) {
    pid_t return_val;
    lock_acquire(table->pid_lock);
    if (table->num_proc + 1 > PID_MAX) {
        //do something
    }
    table->next_pid++;
    return_val = table->next_pid;
    table->num_proc++;
    lock_release(table->pid_lock);
    return return_val;
}

void remove_pid(void) {
    lock_acquire(table->pid_lock);
    table->num_proc--;
    lock_release(table->pid_lock);
}