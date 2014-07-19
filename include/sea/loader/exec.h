#ifndef __SEA_LOADER_EXEC_H
#define __SEA_LOADER_EXEC_H

#include <sea/tm/process.h>

int execve(char *path, char **argv, char **env);
int loader_do_shebang(int desc, char **argv, char **env);
int do_exec(task_t *t, char *path, char **argv, char **env, int shebanged);

#endif
