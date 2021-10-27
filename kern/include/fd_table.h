#include <types.h>
#include <synch.h>

#ifndef _FDTABLE_H_
#define _FDTABLE_H_

#define MAX_FILE_NAME 50
#define MAX_OPEN_FILES 32

struct fd_table
{
    struct fobj *files[32];
    struct lock *fd_table_lk;
};

struct fobj {
     struct vnode *vn;
     mode_t mode;
     off_t offset;
     int refcount;
     struct lock *fobj_lk;
};

/* Searches the fd_table for the first null entry and inserts the new file object there. Returns
the 0 if the insertion was successful, and returns -1 error if the file table is currently full.
*/
int fd_table_add(struct fd_table *table, struct fobj *newfile, int *fd);

/*Removes an entry from file descriptor table, means closing the file*/
int fd_table_remove(struct fd_table *table, int fd);

/*Given the file descriptor fd as an argument gets the file that fd is pointing to which is also indexed by fd*/
int fd_table_get(struct fd_table *table, int fd, struct fobj **file);

/*Creates a new file descriptors table*/
int fd_table_create(struct fd_table **table);

#endif
