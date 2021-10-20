#include <kern/fd_table.h>

struct fd_table* 
fd_table_create(void) {
    struct fd_table *table;

    table = kmalloc(sizeof(struct fd_table));
    KASSERT(table != NULL);

    //Set each entry in our file table to NULL as initially all files are not open
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        table->files[i] = NULL;
    }
    table->fd_table_lk = lock_create("fd_table");

    //TODO: Initialize fd = 0, 1, 2 (stdin, stdout, err)
}

/* Searches the fd_table for the first null entry and inserts the new file object there. Returns
the 0 if the insertion was successful, and returns -1 error if the file table is currently full.
*/
int fd_table_add(struct fd_table *table, struct fobj *newfile, int *fd) {

    lock_acquire(table->fd_table_lk);
    for (int i = 0; i < 32; i++){
        if(table->files[i] == NULL){
            table->files[i] = newfile;
            *fd = i;
            lock_release(table->fd_table_lk);
            //No error
            return 0;
        }
    }
    lock_release(table->fd_table_lk);
    //Error for when a table is full
    return -1;
}


int fd_table_remove(struct fd_table *table, int fd) {
    struct *fobj;

    KASSERT(table != NULL);
    KASSERT(fd >= 0);
    KASSERT(fd < MAX_OPEN_FILES);
    
    lock_acquire(fd_table->fd_table_lk);
    if (fd_table->files[fd] == NULL) {
        lock_release(fd_table->fd_table_lk)
        return -1;
    }

    fobj = fd_table->files[fd];
    table->files[fd] = NULL;

    return 0;
}

//int sys_open(const char *filename, int flags, mode_t mode);
