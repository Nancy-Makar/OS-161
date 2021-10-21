#include <fd_table.h>
#include <kern/errno.h>
#include <types.h>
#include <spl.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <vfs.h>
#include <synch.h>
#include <kern/fcntl.h>


int fd_table_create(struct fd_table **fd_table) {
    int err;

    struct fd_table *table = kmalloc(sizeof(struct fd_table));
    KASSERT(table != NULL);

    //Set each entry in our file table to NULL as initially all files are not open
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        table->files[i] = NULL;
    }
    table->fd_table_lk = lock_create("fd_table");

    lock_acquire(table->fd_table_lk);
    
    //Create STDIN
    char path[] = "con:\0";
    struct fobj *stdin = kmalloc(sizeof(struct fobj));
    struct vnode *vn_stdin;
    err = vfs_open((char*) path, O_RDONLY, 0, &vn_stdin);
    if (err) {
        return err;
    }
    stdin->vn = vn_stdin;
    stdin->mode = 0;
    stdin->refcount = 1;
    stdin->fobj_lk = lock_create("stdin");
    table->files[0] = stdin;

    struct fobj *stderr = kmalloc(sizeof(struct fobj));
    struct vnode *vn_stderr;
    err = vfs_open((char*) path, O_WRONLY, 0, &vn_stderr);
    if (err) {
        return err;
    }
    stderr->vn = vn_stderr;
    stderr->mode = 0;
    stderr->refcount = 1;
    stderr->fobj_lk = lock_create("stderr");
    table->files[2] = stderr;

    struct fobj *stdout = kmalloc(sizeof(struct fobj));
    struct vnode *vn_stdout;
    err = vfs_open((char*) path, O_WRONLY, 0, &vn_stdout);
    if (err) {
        return err;
    }
    stdout->vn = vn_stdout;
    stdout->mode = 0;
    stdout->refcount = 1;
    stdout->fobj_lk = lock_create("stdout");
    table->files[1] = stdout;


    lock_release(table->fd_table_lk);

    *fd_table = table; 
    return 0;
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
            return 0;
        }
    }
    lock_release(table->fd_table_lk);
    //Error for when a table is full
    return ENFILE;
}


int fd_table_remove(struct fd_table *fd_table, int fd) {
   
    KASSERT(fd_table != NULL);
    KASSERT(fd >= 0);
    KASSERT(fd < MAX_OPEN_FILES);
    
    lock_acquire(fd_table->fd_table_lk);
    if (fd_table->files[fd] == NULL) {
        lock_release(fd_table->fd_table_lk);
        return EBADF;
    }

    
    fd_table->files[fd] = NULL;

    return 0;
}

int fd_table_get(struct fd_table *fd_table, int fd, struct fobj **fobj)
{
    lock_acquire(fd_table->fd_table_lk);
    if (fd_table->files[fd] == NULL) {
        lock_release(fd_table->fd_table_lk);
        return EBADF;
    } 
    struct fobj *file = fd_table->files[fd];
    lock_release(fd_table->fd_table_lk);
    *fobj = file;
    return 0;
}
