This is the ......(duh duh duh duh).....kernel. *Scary Music*

Technically, ../drivers contain files that get linked here, too, but I don't care.

cpu/:	Code for cpu stuffs
debug/:	The in-kernel debugger
dm/:	Device Management (char, block, ioctl, etc)
fs/:	Code for VFS, filesystem management, abstraction for files, file handle control, and OCRW
loader/:Code for loading files (modules, ELF executables, configuration)
mm/:	Code for the management of memory. vm, pm, heap, paging, etc
tm/:	Code for process management, switching, fork/exit, signals, and IPC
./:	General kernel files
