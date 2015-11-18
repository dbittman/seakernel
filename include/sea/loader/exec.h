#ifndef __SEA_LOADER_EXEC_H
#define __SEA_LOADER_EXEC_H

#include <sea/tm/process.h>
struct file;
int execve(char *path, char **argv, char **env);
int loader_do_shebang(struct file *, char **argv, char **env);
int do_exec(char *path, char **argv, char **env, int shebanged);

#endif

