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
     int offset;
     int refcount;
     struct lock *fobj_lk;
};

int fd_table_add(struct fd_table *table, struct fobj *newfile, int *fd);
int fd_table_remove(struct fd_table *table, int fd);
int fd_table_get(struct fd_table *table, int fd, struct fobj **file);
int fd_table_create(struct fd_table **table);

#endif
