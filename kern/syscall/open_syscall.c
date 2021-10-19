#include <types.h>
#include <clock.h>
#include <copyinout.h>
#include <#include <syscall.h>
#include <fd_table.h>

struct fd_table *table;
int sys_open(const char *filename, int flags, mode_t mode)
{
    struct vnode **vn;
    int err = vfs_open(filename, flags, mode, vn);
    int fd = find_first_entry(table);
    table->array[fd]->p = &vn;
    table->array[fd]->offset = 0;
    table->array[fd]->mode = mode;

    return fd;
}