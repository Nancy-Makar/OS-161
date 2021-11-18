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

struct pid_table *table;
struct pid_obj;

int pid_create(void) {
    table = kmalloc(sizeof(struct pid_table));
    KASSERT(table != NULL);
    for(int i = PID_MIN; i <= PID_MAX; i++){
        table->pid_objs[i] = NULL;
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
        if(table->pid_objs[i] == NULL){ //check for the first bid that has a NULL process
            struct pid_obj *pid_obj = kmalloc(sizeof(struct pid_obj));
            pid_obj->pid_no = i;
            pid_obj->exited = false;
            pid_obj->exitstatus = -1;
            pid_obj->pid_cv = cv_create("pid_cv");
            table->pid_objs[i] = pid_obj;
            lock_release(table->pid_lock);
            return (pid_t)pid_obj->pid_no;
        }
    }
    lock_release(table->pid_lock);
    return -1; //TODO: proper error handling
}

struct pid_obj *get_pid(pid_t pid_no) {
    if (pid_no < 0 || pid_no > PID_MAX) return NULL;
    struct pid_obj *pid_obj = table->pid_objs[pid_no];
    return pid_obj;
}

int pid_exit(pid_t pid_no, int exitcode, bool trap) {
    lock_acquire(table->pid_lock);
    if (table == NULL) {
        return -1;
    }
    struct pid_obj *pid_obj = get_pid(pid_no);
    if (pid_obj == NULL) {
        lock_release(table->pid_lock);
        return 0;
    }
    if (trap)
    {
        cv_broadcast(pid_obj->pid_cv, table->pid_lock);
        lock_release(table->pid_lock);
        int e = _MKWAIT_SIG(exitcode);
        pid_obj->exitstatus = e;
        pid_obj->exited = true;
        return 0;
    }
    //Set the fields for the pid_obj
    int e = _MKWAIT_EXIT(exitcode);
    pid_obj->exitstatus = e;
    pid_obj->exited = true;
    //Wake up any processess waiting for the exit
    cv_broadcast(pid_obj->pid_cv, table->pid_lock);
    lock_release(table->pid_lock);
    return 0;
}

int pid_wait(pid_t pid_no, int *status) {
    lock_acquire(table->pid_lock);
    struct pid_obj *pid_obj = get_pid(pid_no);
    int err;
    if (pid_obj == NULL) {
        return ESRCH; //TODO: error handle
    }
    
    //If pid has not already exited
    while (!pid_obj->exited) {
        cv_wait(pid_obj->pid_cv, table->pid_lock);
    }

    //If pid has exited
    if (pid_obj->exited) {
        if (status) {
            err = copyout(&pid_obj->exitstatus, (userptr_t) status, sizeof(int));
            if (err) {
                lock_release(table->pid_lock);
                return err;
            }
        }
    }
    lock_release(table->pid_lock);
    return 0;
}




