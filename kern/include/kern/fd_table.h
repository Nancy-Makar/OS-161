#include <types.h>

#ifndef _FDTABLE_H_
#define _FDTABLE_H_

struct fd_table
{
    fobj *array[32];
    lock *fdtable_lk;

 };

 struct fobj {
     struct vnode *p;
     mode_t mode;
     int offset;
 };

 int find_first_entry(struct fd_table *table);

#endif
