Functions for file access.

./: Contains functions that act as a wrapper for the VFS, and an interface to the userspace libraries. 
vfs/: Contains functions for creating a linked-list tree that acts as a wrapper for filesystem drivers.

[devfs+procfs].c: Provides access to devices and the kernel, respectivly. They are in-kernel filesystem drivers.
ramfs.c: Another in-kernel filesystem driver. Most basic wrapper to the VFS tree that allows file access.
