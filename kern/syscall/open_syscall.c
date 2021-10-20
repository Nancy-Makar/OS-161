#include <types.h>
#include <lib.h>
#include <clock.h>
#include <synch.h>
#include <copyinout.h>
#include <syscall.h>
#include <fd_table.h>
#include <vfs.h>
#include <proc.h>

int sys_open(const char *filename, int flags, mode_t mode, int *fd)
{
    struct      proc *p;
    struct      fd_table *table;
    struct      fobj *newfile;
    char        fobjname[MAX_FILE_NAME];
    int         err;

    //Find the current process
    p = currthread->td_proc;
    
    spinlock_acquire(p->lock);

    //If the current file table is null, we know we have to create it
    if (p->fd_table == NULL) {
        p->fd_table = fd_table_create(void);
    }
    table = p->fd_table;
    spinlock_release(p->lock);

    //Take care opening the file name in the kernel
    err = copyinstr(filename, fobjname, sizeof(fobjname), NULL);
    if (err) {
        return err;
    }

    //Open the file and get the associated vnode
    struct vnode *vn;
    err = vfs_open(filename, flags, mode, &vn);
    if (err) {
        return err;
    }

    //Create a new file object
    newfile = kmalloc(sizeof(struct fobj));
    newfile->vn = vn;
    newfile->mode = mode;
    newfile->offset = 0;
    newfile->refcount = 1;
    newfile->fobj_lk = lock_create("fileobjectlk");
    
    //Add our new entry into the table
    err = fd_table_add(fd, newfile, fd);
    if (err) {
        return err;
    }

    return 0;
}

int sys_close(int fd) {    

    struct proc     *p;
    struct fd_table *table;
    struct fobj     *file;
    int             err;

    p = currthread->td_proc;

    spinlock_acquire(p->lock);
    table = p->table;
    spinlock_release(p->lock);

    file = fd_table_get(table, fd);
    if (err) {
        return err;
    } 
    //Remove the entry from the table
    fd_table_remove(table, fd);
    lock_acquire(file->fobj_lk);

    //Close the VNODE
    vfs_close(file->vn);

    //Decrement refrence count
    file->refcount--;

    //If refrence count is not 0, we know that it is being used by another file descriptor!
    if (file->refcount != 0) {
        lock_release(file->fobj_lk);
        return 0;
    }
    //Else, we know its safe to delete
    lock_release(file->fobj_lk);
    lock_destroy(file->fobj_lk);
    kfree(file);

    return 0;
}