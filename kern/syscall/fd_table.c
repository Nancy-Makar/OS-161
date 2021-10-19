#include <fd_table.h>


int find_first_entry(struct fd_table *table){

    lock_acquire(table->fdtable_lk);
    for (int i; i < 32; i++){
        if(table->array[i] == NULL){
            lock_release(table->fdtable_lk);
            return i;
        }
    }
    lock_release(table->fdtable_lk);
    return -1;
}

//int sys_open(const char *filename, int flags, mode_t mode);
