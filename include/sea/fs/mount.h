#ifndef __SEA_FS_MOUNT_H
#define __SEA_FS_MOUNT_H

#include <sea/fs/inode.h>

#include <sea/types.h>
#include <sea/sys/stat.h>
int sys_mount(char *node, char *to);
int sys_mount2(char *node, char *to, char *name, char *opts, int);
#endif

