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


/*Creates a new file descriptors table*/
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
    char path1[] = "con:";
    struct fobj *stdin = kmalloc(sizeof(struct fobj));
    struct vnode *vn_stdin;
    err = vfs_open((char*) path1, O_RDONLY, 0, &vn_stdin);
    if (err) {
        return err;
    }
    stdin->vn = vn_stdin;
    stdin->mode = 0;
    stdin->refcount = 1;
    stdin->fobj_lk = lock_create("stdin");
    table->files[0] = stdin;

    char path2[] = "con:";
    struct fobj *stderr = kmalloc(sizeof(struct fobj));
    struct vnode *vn_stderr;
    err = vfs_open((char*) path2, O_WRONLY, 0, &vn_stderr);
    if (err) {
        return err;
    }
    stderr->vn = vn_stderr;
    stderr->mode = 0;
    stderr->refcount = 1;
    stderr->fobj_lk = lock_create("stderr");
    table->files[2] = stderr;

    char path3[] = "con:";
    struct fobj *stdout = kmalloc(sizeof(struct fobj));
    struct vnode *vn_stdout;
    err = vfs_open((char*) path3, O_WRONLY, 0, &vn_stdout);
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
    if (*fd >= MAX_OPEN_FILES || *fd < 0) 
    {
        lock_release(table->fd_table_lk);
        return EBADF;
    }
    for (int i = 0; i < 32; i++){
        if(table->files[i] == NULL){
            table->files[i] = newfile; //gives a deadlock (weird)
            *fd = i;
            lock_release(table->fd_table_lk);
            return 0;
        }
    }
    lock_release(table->fd_table_lk);
    //Error for when a table is full
    return EMFILE;
}

/*Removes an entry from file descriptor table, means closing the file*/
int fd_table_remove(struct fd_table *fd_table, int fd) {
       
    lock_acquire(fd_table->fd_table_lk);
    if (fd >= MAX_OPEN_FILES || fd < 0) 
    {
        lock_release(fd_table->fd_table_lk);
        return EBADF;
    }
    if (fd_table->files[fd] == NULL) {
        lock_release(fd_table->fd_table_lk);
        return EBADF;
    }
    fd_table->files[fd] = NULL;
    lock_release(fd_table->fd_table_lk);
    return 0;
}

/*Given the file descriptor fd as an argument gets the file that fd is pointing to which is also indexed by fd*/
int fd_table_get(struct fd_table *fd_table, int fd, struct fobj **fobj)
{
    lock_acquire(fd_table->fd_table_lk);
    if (fd >= MAX_OPEN_FILES || fd < 0) 
    {
        lock_release(fd_table->fd_table_lk);
        return EBADF;
    }
    if (fd_table->files[fd] == NULL) {
        lock_release(fd_table->fd_table_lk);
        return EBADF;
    } 
    struct fobj *file = fd_table->files[fd];
    lock_release(fd_table->fd_table_lk);
    *fobj = file;
    return 0;
}

void fd_table_copy(struct fd_table *fd_table, struct fd_table **new_fd_table){
    struct fd_table *table = kmalloc(sizeof(struct fd_table));
    KASSERT(table != NULL);
    table->fd_table_lk = lock_create("fd_table");
    lock_acquire(table->fd_table_lk);
    for (int i = 0; i < MAX_OPEN_FILES; i++){
        table->files[i] = fd_table->files[i];
        table->files[i]->refcount++;
        vnode_incref(table->files[i]->vn);
    }
    lock_release(table->fd_table_lk);

    *new_fd_table = table;
}
