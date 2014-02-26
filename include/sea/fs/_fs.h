#ifndef _SEA_FS__FS_H
#define _SEA_FS__FS_H

#include <sea/subsystem.h>

#if SUBSYSTEM != _SUBSYSTEM_FS
#error "_fs.h included from a non-fs source file"
#endif

#define FPUT_CLOSE 1

#endif
