#include <types.h>
#include <synch.h>

#ifndef _FDTABLE_H_
#define _FDTABLE_H_

#define MAX_FILE_NAME = 50;
#define MAX_OPEN_FILE = 32;

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

 int fd_table_add(int fd, struct fobj *fobj);

 int fd_table_remove(int fd);

 struct fd_table* fd_table_create(void);

#endif
