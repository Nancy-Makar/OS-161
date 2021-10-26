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
#include <errno.h>
#include <kern/fcntl.h>
#include <kern/seek.h>
#include <kern/stat.h>

int sys_open(const_userptr_t filename, int flags, mode_t mode, int32_t *fd)
{
    struct      proc *p;
    struct      fd_table *table;
    struct      fobj *newfile;
    char        fobjname[MAX_FILE_NAME];
    int         err;

    //Find the current process
    p = curthread->t_proc;
    
    //If the current file table is null, we know we have to create it
    spinlock_acquire(&p->p_lock);
    if (p->p_fdtable == NULL)
    {
        err = fd_table_create(&table);
        if (err) {
            return err;
        }
        p->p_fdtable = table;
    } else {
        table = p->p_fdtable;
    }
    spinlock_release(&p->p_lock);

    //Take care opening the file name in the kernel
    err = copyinstr(filename, fobjname, sizeof(fobjname), NULL);
    if (err) {
        return err;
    }

    //Open the file and get the associated vnode
    struct vnode *vn;
    err = vfs_open(fobjname, flags, mode, &vn);
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
    err = fd_table_add(table, newfile, fd);
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

    p = curthread->t_proc;

    spinlock_acquire(&p->p_lock);
    table = p->p_fdtable;
    spinlock_release(&p->p_lock);

    err = fd_table_get(table, fd, &file);

    if (err) {
        return err;
    }
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

int sys_read(int fd, void *buf, size_t buflen, int32_t *count) {

    struct proc     *p;
    struct fd_table *table;
    struct fobj     *file;
    int             err;
    char *kbuf = (char*) kmalloc(buflen);

    //Ensures buf is safe and stores it into kbuf
    err = copyin((const_userptr_t) buf, kbuf, buflen);
    if (err) {
        return err;
    }

    p = curthread->t_proc;

    //If the current file table is null, we know we have to create it
    spinlock_acquire(&p->p_lock);
    if (p->p_fdtable == NULL)
    {
        err = fd_table_create(&table);
        if (err) {
            return err;
        }
        p->p_fdtable = table;
    } else {
        table = p->p_fdtable;
    }
    spinlock_release(&p->p_lock);

    err = fd_table_get(table, fd, &file);
    if (err) {
        return err;
    }

    struct iovec f_iovec;
	struct uio f_uio;
    
    lock_acquire(file->fobj_lk);

    //TODO: Figure this out!
    // //Wrong permissions
    // if (file->mode > 64 ||  ) { //Applicable only for the first 3 modes
    //     lock_release(file->fobj_lk);
    //     return EBADF;
    // }

    // switch(file->mode){
    //     case O_RDONLY:
    //         break;
    //     case O_WRONLY:
    //         break;
    //     case O_RDWR:
    //         break;
    //     case O_CREAT:
    //         break;
    //     case O_EXCL:
    //         break;
    //     case O_TRUNC:
    //         break;
    //     case O_APPEND:
    //         break;
    //     case O_NOCTTY:
    //         break;
    //     case O_ACCMODE:
    //         break;
    //     default:
    //         lock_release(file->fobj_lk);
    //         return EBADF;
    // }


	uio_kinit(&f_iovec, &f_uio, kbuf, buflen, (off_t) file->offset, UIO_READ);

    err = VOP_READ(file->vn, &f_uio);
    if (err) {
        lock_release(file->fobj_lk);
        return err;
    }
    //Amount read = buflen - amount of data remaining in uio
    *count = buflen - f_uio.uio_resid;
    //uio_offset contains our new offset for the file
    file->offset = f_uio.uio_offset;

    lock_release(file->fobj_lk);

    err = copyout(kbuf, buf, *count); 
    kfree(kbuf);
    if (err) {
        return err;
    }

    return 0;
}

int sys_write(int fd, void *buf, size_t nbytes, int32_t *count) {
    struct proc     *p;
    struct fd_table *table;
    struct fobj     *file;
    int             err;
    char *kbuf = (char*) kmalloc(nbytes);

    //Ensures buf is safe and stores it into kbuf
    err = copyin((const_userptr_t) buf, kbuf, nbytes);
    if (err) {
        return err;
    }

    p = curthread->t_proc;

    //If the current file table is null, we know we have to create it
    spinlock_acquire(&p->p_lock);
    if (p->p_fdtable == NULL)
    {
        err = fd_table_create(&table);
        if (err) {
            return err;
        }
    } else {
        table = p->p_fdtable;
    }
    spinlock_release(&p->p_lock);

    err = fd_table_get(table, fd, &file);
    if (err) {
        return err;
    }

    struct iovec f_iovec;
	struct uio f_uio;
    
    lock_acquire(file->fobj_lk);

    //Wrong permissions
    // if (file->mode == O_RDONLY)
    // {
    //     lock_release(file->fobj_lk);
    //     return EBADF;
    // }
    uio_kinit(&f_iovec, &f_uio, kbuf, nbytes, (off_t) file->offset, UIO_WRITE);

    err = VOP_WRITE(file->vn, &f_uio);
    if (err) {
        lock_release(file->fobj_lk);
        return err;
    }
    //Amount read = buflen - amount of data remaining in uio
    *count = nbytes - f_uio.uio_resid;
    //uio_offset contains our new offset for the file
    file->offset = f_uio.uio_offset;

    lock_release(file->fobj_lk);

    return 0;
}


int sys_lseek(int fd, off_t pos, int whence, off_t *new_pos) { //whence user pointer



    struct proc *p;
    struct fd_table *table;
    struct fobj *file;
    int err;

    p = curthread->t_proc;



    spinlock_acquire(&p->p_lock);
    table = p->p_fdtable;
    err = fd_table_get(table, fd, &file);
    if (err)
    {
        spinlock_release(&p->p_lock);
        return err; //EBADF
    }
    spinlock_release(&p->p_lock);

    struct stat statbuf;
    lock_acquire(file->fobj_lk);


    switch (whence)
    {
    case SEEK_SET:
        *new_pos = pos;
        break;
    case SEEK_CUR:
        *new_pos = file->offset + pos;
        break;
    case SEEK_END:
        err = VOP_STAT(file->vn, &statbuf);
        if(err){
            lock_release(file->fobj_lk);
            return err;
        }
        *new_pos = statbuf.st_size + pos;
        break;
    default :
        lock_release(file->fobj_lk);
        return EINVAL;
    }

    if (*new_pos < 0)
    {
        lock_release(file->fobj_lk);
        return EINVAL;
    }
    file->offset = *new_pos;
    if (!VOP_ISSEEKABLE(file->vn))
    {
        lock_release(file->fobj_lk);
        return ESPIPE;
    }
    lock_release(file->fobj_lk);
    return 0;
}

int sys_dup2(int oldfd, int newfd, int *newfdreturn) {
    struct proc *p;
    struct fd_table *table;
    struct fobj *file;
    struct fobj *file2;
    int err;

    if (newfd >= MAX_OPEN_FILES || newfd < 0){
         return EBADF;
    }
    if (oldfd >= MAX_OPEN_FILES || oldfd < 0){
        return EBADF;
    }

    if(oldfd == newfd){
        *newfdreturn = oldfd;
        return oldfd;
    }

    p = curthread->t_proc;

    spinlock_acquire(&p->p_lock);
    table = p->p_fdtable;
    spinlock_release(&p->p_lock);
    err = fd_table_get(table, oldfd, &file);
    if (err) {
        return err;
    }
    //Check if newfd points to an open file 
    if(!fd_table_get(table, newfd, &file2)){
        //If it does point to an open file, lets close that file
        err = sys_close(newfd);
        if(err){
           return err;
       }
    }
    //Increase the reference count
    lock_acquire(file->fobj_lk);
    file->refcount++;
    VOP_INCREF(file->vn);
    lock_release(file->fobj_lk);

    err = fd_table_add(table, file, &newfd);
    if(err){
        return err;
    }
    *newfdreturn = newfd;
    return 0;
}

int sys_chdir(const_userptr_t pathname)
{

    if (pathname == NULL)
    {
        return EFAULT;
    }

    char fobjname[MAX_FILE_NAME];
    int err;


    err = copyinstr(pathname, fobjname, sizeof(fobjname), NULL);

    if(err){
        return err;
    }

    err = vfs_chdir((char*)pathname); 
    if(err){
        return err;
    }
    return 0;
}

int sys_getcwd(userptr_t buf, size_t buflen, int *dataread){

    int err;
    char *kbuf = (char*) kmalloc(buflen);

    //Ensures buf is safe and stores it into kbuf
    err = copyin((const_userptr_t) buf, kbuf, buflen);
    if (err) {
        return err;
    }


    struct uio f_uio;
    struct iovec f_iovec;

    uio_kinit(&f_iovec, &f_uio, kbuf, buflen, 0, UIO_READ);

    err = vfs_getcwd(&f_uio);
    if(err){
        return err;
    }

   //Amount read = buflen - amount of data remaining in uio
    *dataread = buflen - f_uio.uio_resid;

    err = copyout(kbuf, (userptr_t) buf, *dataread); 
    kfree(kbuf);
    if (err) {
        return err;
    }
    return 0;
}